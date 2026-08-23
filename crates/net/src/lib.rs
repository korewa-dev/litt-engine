//! Litt Engine networking — real UDP/TCP transport for multiplayer and
//! multi-agent sessions.
//!
//! Design goals:
//! - Zero heavy dependencies (`std::net` only)
//! - Non-blocking receive so the game loop never stalls
//! - Length-prefixed framing over TCP; single-datagram messages over UDP
//! - Snapshot helpers for transform replication out of the box
//!
//! ```ignore
//! use litt_net::*;
//!
//! // Host
//! let mut server = NetServer::bind(Transport::Tcp, "127.0.0.1:7777")?;
//! // Client (other machine / agent process)
//! let mut client = NetClient::connect(Transport::Tcp, "127.0.0.1:7777")?;
//! client.send(&Message::new(1, b"hello"))?;
//! if let Some((peer, msg)) = server.recv_nonblocking() {
//!     server.broadcast(&Message::new(2, &msg.payload))?;
//! }
//! ```

use std::io::{Read, Write};
use std::net::{SocketAddr, TcpListener, TcpStream, ToSocketAddrs, UdpSocket};
use std::sync::mpsc::Receiver;
use std::sync::{Arc, Mutex};

// =============================================================================
// Messages & framing
// =============================================================================

/// Default topics used by engine systems.
pub mod topics {
    /// Text/chat/debug messages
    pub const TEXT: u16 = 1;
    /// Transform snapshot batch ([`crate::TransformSnapshot`])
    pub const SNAPSHOTS: u16 = 2;
    /// Input events (agent remote control)
    pub const INPUT: u16 = 3;
    /// Scene/world events
    pub const EVENT: u16 = 4;
}

/// Hard cap on a single TCP frame (header + payload). Protects readers from
/// runaway buffering on malformed or hostile streams. UDP datagrams are
/// already capped by the socket MTU (~64 KiB).
pub const MAX_FRAME_SIZE: usize = 16 * 1024 * 1024;

/// A single network message: 16-bit topic + opaque payload.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Message {
    pub topic: u16,
    pub payload: Vec<u8>,
}

impl Message {
    pub fn new(topic: u16, payload: &[u8]) -> Self {
        Self { topic, payload: payload.to_vec() }
    }

    pub fn text(topic: u16, text: &str) -> Self {
        Self::new(topic, text.as_bytes())
    }

    /// Decode payload as UTF-8 text lossily.
    pub fn as_text(&self) -> String {
        String::from_utf8_lossy(&self.payload).into_owned()
    }

    /// TCP frame: u32 BE total-len | u16 BE topic | payload
    pub fn encode_framed(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(6 + self.payload.len());
        out.extend_from_slice(&((self.payload.len() + 2) as u32).to_be_bytes());
        out.extend_from_slice(&self.topic.to_be_bytes());
        out.extend_from_slice(&self.payload);
        out
    }

    /// Parse one full frame from `buf`.
    /// Returns `(message, bytes_consumed)` or None if incomplete.
    pub fn decode_framed(buf: &[u8]) -> Option<(Message, usize)> {
        if buf.len() < 6 {
            return None;
        }
        let total = u32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]]) as usize;
        if total < 2 || total > MAX_FRAME_SIZE || buf.len() < 4 + total {
            return None;
        }
        let topic = u16::from_be_bytes([buf[4], buf[5]]);
        Some((
            Message { topic, payload: buf[6..4 + total].to_vec() },
            4 + total,
        ))
    }

    /// UDP datagram layout: u16 BE topic | payload
    pub fn encode_datagram(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(2 + self.payload.len());
        out.extend_from_slice(&self.topic.to_be_bytes());
        out.extend_from_slice(&self.payload);
        out
    }

    pub fn decode_datagram(buf: &[u8]) -> Option<Message> {
        if buf.len() < 2 {
            return None;
        }
        Some(Message {
            topic: u16::from_be_bytes([buf[0], buf[1]]),
            payload: buf[2..].to_vec(),
        })
    }
}

/// Underlying transport protocol.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Transport {
    Udp,
    Tcp,
}

// =============================================================================
// Wire helpers
// =============================================================================

fn rd_u32(buf: &[u8], pos: &mut usize) -> u32 {
    let v = u32::from_be_bytes([buf[*pos], buf[*pos + 1], buf[*pos + 2], buf[*pos + 3]]);
    *pos += 4;
    v
}

fn rd_f32(buf: &[u8], pos: &mut usize) -> f32 {
    f32::from_bits(rd_u32(buf, pos))
}

// =============================================================================
// Client
// =============================================================================

/// Network client — connects to a [`NetServer`] and exchanges messages.
pub struct NetClient {
    transport: Transport,
    tcp: Option<TcpStream>,
    udp: Option<Arc<UdpSocket>>,
    remote: SocketAddr,
    inbox: Receiver<Message>,
    _reader: std::thread::JoinHandle<()>,
}

impl NetClient {
    /// Connect to a server. For UDP this targets the remote address;
    /// there is no handshake — the first packet opens the flow.
    pub fn connect(transport: Transport, addr: &str) -> Result<Self, String> {
        let remote = addr
            .to_socket_addrs()
            .map_err(|e| format!("resolve '{}' failed: {}", addr, e))?
            .next()
            .ok_or_else(|| format!("resolve '{}' produced no addresses", addr))?;

        match transport {
            Transport::Tcp => {
                let stream = TcpStream::connect(remote)
                    .map_err(|e| format!("tcp connect {} failed: {}", remote, e))?;
                let write_half = stream.try_clone()
                    .map_err(|e| format!("stream clone failed: {}", e))?;
                let (tx, rx) = std::sync::mpsc::channel();
                let reader = spawn_tcp_reader(stream, tx);
                Ok(Self {
                    transport,
                    tcp: Some(write_half),
                    udp: None,
                    remote,
                    inbox: rx,
                    _reader: reader,
                })
            }
            Transport::Udp => {
                let socket = UdpSocket::bind("0.0.0.0:0")
                    .map_err(|e| format!("udp bind failed: {}", e))?;
                socket
                    .connect(remote)
                    .map_err(|e| format!("udp connect {} failed: {}", remote, e))?;
                let sock = Arc::new(socket);
                let (tx, rx) = std::sync::mpsc::channel();
                let reader_sock = sock.clone();
                let reader = std::thread::spawn(move || {
                    let mut buf = [0u8; 65_507];
                    while let Ok(n) = reader_sock.recv(&mut buf) {
                        match Message::decode_datagram(&buf[..n]) {
                            Some(msg) => {
                                if tx.send(msg).is_err() { break; }
                            }
                            None => continue,
                        }
                    }
                });
                Ok(Self {
                    transport,
                    tcp: None,
                    udp: Some(sock),
                    remote,
                    inbox: rx,
                    _reader: reader,
                })
            }
        }
    }

    pub fn transport(&self) -> Transport { self.transport }
    pub fn remote_addr(&self) -> SocketAddr { self.remote }

    /// Send a message (blocking write; payloads are small by design).
    pub fn send(&mut self, msg: &Message) -> Result<(), String> {
        if let Some(stream) = &mut self.tcp {
            stream
                .write_all(&msg.encode_framed())
                .map_err(|e| format!("tcp send failed: {}", e))
        } else if let Some(sock) = &self.udp {
            sock.send(&msg.encode_datagram())
                .map(|_| ())
                .map_err(|e| format!("udp send failed: {}", e))
        } else {
            Err("client not connected".to_string())
        }
    }

    /// Poll the inbox without blocking. Returns None when empty.
    pub fn recv_nonblocking(&self) -> Option<Message> {
        self.inbox.try_recv().ok()
    }
}

// =============================================================================
// Server
// =============================================================================

type PeerStreams = Arc<Mutex<Vec<(SocketAddr, TcpStream)>>>;

/// Network server — accepts clients and fans messages out.
pub struct NetServer {
    transport: Transport,
    tcp_peers: PeerStreams,
    udp_socket: Option<Arc<UdpSocket>>,
    udp_peers: Arc<Mutex<Vec<SocketAddr>>>,
    local_addr: SocketAddr,
    inbox: Receiver<(SocketAddr, Message)>,
    _threads: Vec<std::thread::JoinHandle<()>>,
}

impl NetServer {
    /// Bind a server on `addr` (e.g. `"0.0.0.0:7777"`).
    pub fn bind(transport: Transport, addr: &str) -> Result<Self, String> {
        match transport {
            Transport::Tcp => {
                let listener = TcpListener::bind(addr)
                    .map_err(|e| format!("tcp bind '{}' failed: {}", addr, e))?;
                let local_addr = listener.local_addr()
                    .map_err(|e| format!("local_addr failed: {}", e))?;

                let peers: PeerStreams = Arc::new(Mutex::new(Vec::new()));
                let (tx, rx) = std::sync::mpsc::channel();
                let mut threads = Vec::new();

                // Acceptor thread: register each client, spawn its reader
                let acceptor_peers = peers.clone();
                threads.push(std::thread::spawn(move || {
                    for stream in listener.incoming() {
                        let Ok(stream) = stream else { break };
                        let Ok(peer) = stream.peer_addr() else { continue };
                        let Ok(read_half) = stream.try_clone() else { continue };

                        acceptor_peers.lock().unwrap().push((peer, stream));

                        let tx = tx.clone();
                        std::thread::spawn(move || {
                            forward_tcp_messages(read_half, |msg| tx.send((peer, msg)).is_ok())
                        });
                    }
                }));

                Ok(Self {
                    transport,
                    tcp_peers: peers,
                    udp_socket: None,
                    udp_peers: Arc::new(Mutex::new(Vec::new())),
                    local_addr,
                    inbox: rx,
                    _threads: threads,
                })
            }
            Transport::Udp => {
                let socket = UdpSocket::bind(addr)
                    .map_err(|e| format!("udp bind '{}' failed: {}", addr, e))?;
                let local_addr = socket.local_addr()
                    .map_err(|e| format!("local_addr failed: {}", e))?;
                let sock = Arc::new(socket);
                let udp_peers: Arc<Mutex<Vec<SocketAddr>>> = Arc::new(Mutex::new(Vec::new()));

                let (tx, rx) = std::sync::mpsc::channel();
                let reader_sock = sock.clone();
                let reader_peers = udp_peers.clone();
                let reader = std::thread::spawn(move || {
                    let mut buf = [0u8; 65_507];
                    loop {
                        match reader_sock.recv_from(&mut buf) {
                            Ok((n, peer)) => {
                                {
                                    let mut k = reader_peers.lock().unwrap();
                                    if !k.contains(&peer) {
                                        k.push(peer);
                                    }
                                }
                                if let Some(msg) = Message::decode_datagram(&buf[..n]) {
                                    if tx.send((peer, msg)).is_err() {
                                        break;
                                    }
                                }
                            }
                            Err(_) => break,
                        }
                    }
                });

                Ok(Self {
                    transport,
                    tcp_peers: Arc::new(Mutex::new(Vec::new())),
                    udp_socket: Some(sock),
                    udp_peers,
                    local_addr,
                    inbox: rx,
                    _threads: vec![reader],
                })
            }
        }
    }

    pub fn transport(&self) -> Transport { self.transport }
    pub fn local_addr(&self) -> SocketAddr { self.local_addr }

    /// Number of connected/known peers.
    pub fn peer_count(&self) -> usize {
        match &self.udp_socket {
            None => self.tcp_peers.lock().unwrap().len(),
            Some(_) => self.udp_peers.lock().unwrap().len(),
        }
    }

    /// Poll one inbound message. Returns `(sender, message)` or None.
    pub fn recv_nonblocking(&self) -> Option<(SocketAddr, Message)> {
        self.inbox.try_recv().ok()
    }

    /// Send to every connected peer. Returns count of successful sends.
    pub fn broadcast(&self, msg: &Message) -> Result<usize, String> {
        if let Some(sock) = &self.udp_socket {
            let datagram = msg.encode_datagram();
            let peers = self.udp_peers.lock().unwrap().clone();
            let mut sent = 0usize;
            for peer in peers {
                if sock.send_to(&datagram, peer).is_ok() {
                    sent += 1;
                }
            }
            return Ok(sent);
        }

        let frame = msg.encode_framed();
        let mut peers = self.tcp_peers.lock().unwrap();
        let mut sent = 0usize;
        let mut dead = Vec::new();
        for (i, (_, stream)) in peers.iter_mut().enumerate() {
            if stream.write_all(&frame).is_ok() {
                sent += 1;
            } else {
                dead.push(i); // prune after the loop to keep order stable
            }
        }
        for i in dead.into_iter().rev() {
            peers.swap_remove(i);
        }
        Ok(sent)
    }

    /// Send directly to one UDP peer address.
    pub fn send_to(&self, peer: SocketAddr, msg: &Message) -> Result<(), String> {
        match &self.udp_socket {
            Some(sock) => sock
                .send_to(&msg.encode_datagram(), peer)
                .map(|_| ())
                .map_err(|e| format!("udp send_to failed: {}", e)),
            None => Err("send_to is UDP-only; use broadcast for TCP".to_string()),
        }
    }
}

// =============================================================================
// Thread helpers
// =============================================================================

fn spawn_tcp_reader(
    stream: TcpStream,
    tx: std::sync::mpsc::Sender<Message>,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || {
        forward_tcp_messages(stream, move |msg| tx.send(msg).is_ok())
    })
}

/// Read length-prefixed frames until EOF/error; invoke `sink` per message.
fn forward_tcp_messages(mut stream: TcpStream, mut sink: impl FnMut(Message) -> bool) {
    let mut buf: Vec<u8> = Vec::with_capacity(4096);
    let mut chunk = [0u8; 4096];

    'outer: loop {
        // Protocol guard: a header claiming more than MAX_FRAME_SIZE bytes is
        // a malformed/hostile stream -- drop the connection instead of
        // buffering toward it forever.
        if buf.len() >= 4 {
            let claimed = u32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]]) as usize;
            if claimed > MAX_FRAME_SIZE {
                return;
            }
        }
        // Parse every complete frame currently buffered
        loop {
            match Message::decode_framed(&buf) {
                Some((msg, consumed)) => {
                    buf.drain(..consumed);
                    if !sink(msg) {
                        return;
                    }
                }
                None => break,
            }
        }
        // Need more bytes
        loop {
            match stream.read(&mut chunk) {
                Ok(0) => break 'outer, // EOF
                Ok(n) => {
                    buf.extend_from_slice(&chunk[..n]);
                    continue 'outer;
                }
                Err(ref e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
                Err(_) => break 'outer,
            }
        }
    }
}

// =============================================================================
// Transform snapshots
// =============================================================================

/// Compact replicated transform (44 bytes on the wire).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TransformSnapshot {
    pub entity_id: u32,
    pub position: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
}

impl TransformSnapshot {
    pub const WIRE_SIZE: usize = 44;

    pub fn encode(&self, out: &mut Vec<u8>) {
        out.extend_from_slice(&self.entity_id.to_be_bytes());
        for v in self.position { out.extend_from_slice(&v.to_be_bytes()); }
        for v in self.rotation { out.extend_from_slice(&v.to_be_bytes()); }
        for v in self.scale { out.extend_from_slice(&v.to_be_bytes()); }
    }

    /// Decode one snapshot starting at `*pos`; advances `*pos`.
    pub fn decode(buf: &[u8], pos: &mut usize) -> Option<Self> {
        if pos.checked_add(Self::WIRE_SIZE)? > buf.len() {
            return None;
        }
        let entity_id = rd_u32(buf, pos);
        let position = [rd_f32(buf, pos), rd_f32(buf, pos), rd_f32(buf, pos)];
        let rotation = [rd_f32(buf, pos), rd_f32(buf, pos), rd_f32(buf, pos), rd_f32(buf, pos)];
        let scale = [rd_f32(buf, pos), rd_f32(buf, pos), rd_f32(buf, pos)];
        Some(Self { entity_id, position, rotation, scale })
    }

    /// Encode a batch into one message payload.
    pub fn encode_batch(snapshots: &[TransformSnapshot]) -> Vec<u8> {
        let mut out = Vec::with_capacity(snapshots.len() * Self::WIRE_SIZE);
        for s in snapshots {
            s.encode(&mut out);
        }
        out
    }

    pub fn decode_batch(buf: &[u8]) -> Vec<TransformSnapshot> {
        let mut out = Vec::new();
        let mut pos = 0usize;
        while let Some(s) = Self::decode(buf, &mut pos) {
            out.push(s);
        }
        out
    }

    /// Build a ready-to-send snapshots message.
    pub fn batch_message(snapshots: &[TransformSnapshot]) -> Message {
        Message { topic: topics::SNAPSHOTS, payload: Self::encode_batch(snapshots) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    fn wait_for<T>(mut f: impl FnMut() -> Option<T>) -> T {
        for _ in 0..200 {
            if let Some(v) = f() {
                return v;
            }
            std::thread::sleep(Duration::from_millis(10));
        }
        panic!("condition not met within timeout");
    }

    #[test]
    fn framing_roundtrip() {
        let msg = Message::text(topics::TEXT, "hello net");
        let frame = msg.encode_framed();
        let (decoded, consumed) = Message::decode_framed(&frame).unwrap();
        assert_eq!(consumed, frame.len());
        assert_eq!(decoded, msg);
        assert!(Message::decode_framed(&frame[..frame.len() - 1]).is_none());
    }

    #[test]
    fn datagram_roundtrip() {
        let msg = Message::text(topics::EVENT, "ping");
        assert_eq!(Message::decode_datagram(&msg.encode_datagram()).unwrap(), msg);
    }

    #[test]
    fn oversized_frames_are_rejected() {
        let mut frame = ((MAX_FRAME_SIZE as u32) + 1).to_be_bytes().to_vec();
        assert!(Message::decode_framed(&frame).is_none());

        // Even with the claimed body fully present
        frame.extend(std::iter::repeat(0u8).take(64));
        assert!(Message::decode_framed(&frame).is_none());
    }

    #[test]
    fn tcp_client_server_roundtrip() {
        let mut server = NetServer::bind(Transport::Tcp, "127.0.0.1:0").unwrap();
        let addr = server.local_addr().to_string();

        let mut client = NetClient::connect(Transport::Tcp, &addr).unwrap();
        client.send(&Message::text(topics::TEXT, "hi from agent")).unwrap();

        let (_peer, msg) = wait_for(|| server.recv_nonblocking());
        assert_eq!(msg.as_text(), "hi from agent");

        // Wait until the acceptor registered the peer, then echo
        let _ = wait_for(|| if server.peer_count() > 0 { Some(()) } else { None });
        let sent = server.broadcast(&Message::text(topics::TEXT, "echo")).unwrap_or(0);
        assert_eq!(sent, 1);

        let echo = wait_for(|| client.recv_nonblocking());
        assert_eq!(echo.as_text(), "echo");
        assert_eq!(echo.topic, topics::TEXT);
    }

    #[test]
    fn udp_client_server_roundtrip() {
        let server = NetServer::bind(Transport::Udp, "127.0.0.1:0").unwrap();
        let addr = server.local_addr().to_string();

        let mut client = NetClient::connect(Transport::Udp, &addr).unwrap();
        client.send(&Message::text(topics::INPUT, "move")).unwrap();

        let (peer, msg) = wait_for(|| server.recv_nonblocking());
        assert_eq!(msg.as_text(), "move");

        server.send_to(peer, &Message::text(topics::INPUT, "ack")).unwrap();
        let ack = wait_for(|| client.recv_nonblocking());
        assert_eq!(ack.as_text(), "ack");
    }

    #[test]
    fn snapshot_batch_roundtrip() {
        let snaps = vec![
            TransformSnapshot {
                entity_id: 7,
                position: [1.5, -2.25, 3.0],
                rotation: [0.0, 0.70710678, 0.0, 0.70710678],
                scale: [1.0, 1.0, 1.0],
            },
            TransformSnapshot {
                entity_id: 9,
                position: [0.0, 0.0, 0.0],
                rotation: [0.0, 0.0, 0.0, 1.0],
                scale: [2.0, 2.0, 2.0],
            },
        ];
        let payload = TransformSnapshot::encode_batch(&snaps);
        assert_eq!(payload.len(), snaps.len() * TransformSnapshot::WIRE_SIZE);

        let decoded = TransformSnapshot::decode_batch(&payload);
        assert_eq!(decoded.len(), 2);
        assert_eq!(decoded[0].entity_id, 7);
        assert!((decoded[0].position[1] - (-2.25)).abs() < 1e-6);
        assert!((decoded[1].scale[0] - 2.0).abs() < 1e-6);

        let msg = TransformSnapshot::batch_message(&snaps);
        assert_eq!(msg.topic, topics::SNAPSHOTS);
        assert_eq!(TransformSnapshot::decode_batch(&msg.payload).len(), 2);
    }
}
