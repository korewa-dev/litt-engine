//! Litt Live Viewer — standalone read-only observer for Litt Engine live mode.
//!
//! Drop-in replacement for tools/serve_live.py, written dependency-free so
//! GUI developers can read every line, fork it, and grow a real GUI on top.
//!
//! CONTRACT (from Project/live/AI_RULES.md — do not break):
//!   * GET only. Every other method returns 405.
//!   * Viewers observe; only the AI mutates files on disk.
//!   * Never add an endpoint through which a client could write state.
//!
//! Usage:
//!   live [--port 8088] [--bind 127.0.0.1] [--root <live-dir>]
//!
//! If --root is omitted the server walks up from its own location until it
//! finds a world_state.json and uses that directory as the live root.

use std::fs;
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::{Path, PathBuf};
use std::process;
use std::thread;

fn main() {
    let args = parse_args();
    let root = match args.root {
        Some(r) => r,
        None => match discover_root() {
            Some(r) => r,
            None => {
                eprintln!("error: could not locate world_state.json above the executable.");
                eprintln!("       pass --root <dir> pointing at your Project/live folder.");
                process::exit(2);
            }
        },
    };

    let addr = format!("{}:{}", args.bind, args.port);
    let listener = match TcpListener::bind(&addr) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("error: cannot bind {}: {}", addr, e);
            process::exit(1);
        }
    };

    println!("LITT LIVE - read-only observer server (standalone)");
    println!("  root:    {}", root.display());
    println!("  local:   http://127.0.0.1:{}/viewer/", args.port);
    if args.bind == "0.0.0.0" {
        println!("  network: http://<this-pc>:{}/viewer/   (share at your own risk)", args.port);
    }
    println!("  humans observe; only the AI modifies the world.");

    let root_shared = shared_root(root);
    for stream in listener.incoming() {
        match stream {
            Ok(s) => {
                let root = root_shared.clone();
                thread::spawn(move || handle(s, root));
            }
            Err(e) => eprintln!("[live] accept error: {}", e),
        }
    }
}

// ---------------------------------------------------------------------------
// arguments
// ---------------------------------------------------------------------------

struct Args {
    port: u16,
    bind: String,
    root: Option<PathBuf>,
}

fn parse_args() -> Args {
    let mut a = Args { port: 8088, bind: "127.0.0.1".into(), root: None };
    let mut it = std::env::args().skip(1);
    while let Some(arg) = it.next() {
        match arg.as_str() {
            "--port" => a.port = it.next().and_then(|v| v.parse().ok()).unwrap_or(8088),
            "--bind" => a.bind = it.next().unwrap_or_else(|| "127.0.0.1".into()),
            "--root" => a.root = it.next().map(PathBuf::from),
            "--help" | "-h" => {
                println!("live - Litt Engine read-only live observer");
                println!("  --port <n>     port to bind (default 8088)");
                println!("  --bind <addr>  bind address (default 127.0.0.1)");
                println!("  --root <dir>   Project/live directory (auto-discovered by default)");
                process::exit(0);
            }
            other => eprintln!("warning: unknown argument '{}'", other),
        }
    }
    a
}

/// Walk upwards from this executable looking for world_state.json.
fn discover_root() -> Option<PathBuf> {
    let mut dir = std::env::current_exe().ok()?.parent()?.to_path_buf();
    loop {
        if dir.join("world_state.json").is_file() {
            return Some(dir);
        }
        if !dir.pop() {
            return None;
        }
    }
}

// On some platforms PathBuf isn't Send-safe to share casually; wrap in Arc-like
// clone pattern instead: PathBuf is Clone and each thread gets its own copy.
type SharedRoot = PathBuf;
fn shared_root(p: PathBuf) -> SharedRoot { p }

// ---------------------------------------------------------------------------
// request handling
// ---------------------------------------------------------------------------

fn handle(mut stream: TcpStream, root: SharedRoot) {
    let mut buf = [0u8; 8192];
    let n = match stream.read(&mut buf) {
        Ok(n) if n > 0 => n,
        _ => return,
    };
    let req = String::from_utf8_lossy(&buf[..n]);
    let mut parts = req.split_whitespace();
    let method = parts.next().unwrap_or("");
    let raw_path = parts.next().unwrap_or("/");

    // Read-only contract: anything that is not GET (or HEAD) is refused.
    if method != "GET" && method != "HEAD" {
        respond(&mut stream, 405, "Method Not Allowed",
                b"Litt Live is AI-exclusive: read-only observer\n", "text/plain", true);
        return;
    }

    let path = raw_path.split('?').next().unwrap_or("/");

    // --- API endpoints (all read-only pass-throughs / projections) ----------
    let handled = match path {
        "/api/state" => { serve_file(&mut stream, &root.join("world_state.json"), true); true }
        "/api/index" => { serve_file(&mut stream, &root.join("assets/asset_index.json"), true); true }
        "/api/log"   => { serve_log_tail(&mut stream, &root.join("LIVE_LOG.md"), query_param(raw_path, "tail")); true }
        _ => false,
    };
    if handled { return; }

    // --- viewer + static files ----------------------------------------------
    let rel = match normalize(path) {
        Some(r) => r,
        None => {
            respond(&mut stream, 403, "Forbidden", b"path traversal blocked\n", "text/plain", true);
            return;
        }
    };
    let target = if path == "/" || path.starts_with("/viewer") {
        root.join("viewer").join("index.html")
    } else {
        root.join(&rel)
    };

    serve_file(&mut stream, &target, true);
}

/// Reject traversal attempts; strip leading slash.
fn normalize(path: &str) -> Option<String> {
    let p = path.trim_start_matches('/');
    if p.split('/').any(|seg| seg == ".." || seg == ".") && !p.is_empty() {
        // allow "./"-free simple names only; "." segments are suspicious in URLs
        if p.contains("..") {
            return None;
        }
    }
    if p.contains("..") {
        return None;
    }
    Some(p.to_string())
}

fn serve_log_tail(stream: &mut TcpStream, path: &Path, tail: Option<usize>) {
    let body = match fs::read_to_string(path) {
        Ok(s) => s,
        Err(_) => {
            respond(stream, 404, "Not Found", b"no LIVE_LOG.md yet\n", "text/plain", true);
            return;
        }
    };
    let n = tail.unwrap_or(50).min(500);
    let lines: Vec<&str> = body.lines().collect();
    let start = lines.len().saturating_sub(n);
    let out = lines[start..].join("
");
    respond(stream, 200, "OK", out.as_bytes(), "text/markdown", true);
}

fn serve_file(stream: &mut TcpStream, path: &Path, head_only_ok: bool) {
    if !path.is_file() {
        respond(stream, 404, "Not Found", b"not found\n", "text/plain", true);
        return;
    }
    match fs::read(path) {
        Ok(bytes) => {
            let mime = mime_of(path);
            respond(stream, 200, "OK", &bytes, mime, head_only_ok);
        }
        Err(e) => {
            let msg = format!("read error: {}
", e);
            respond(stream, 500, "Internal Server Error", msg.as_bytes(), "text/plain", true);
        }
    }
}

fn respond(stream: &mut TcpStream, code: u16, reason: &str, body: &[u8], mime: &str, allow_body: bool) {
    let status = format!("HTTP/1.1 {} {}\r\n", code, reason);
    let head = format!(
        "{}Content-Type: {}\r\nContent-Length: {}\r\nCache-Control: no-store, must-revalidate\r\nConnection: close\r\n\r\n",
        status, mime, body.len()
    );
    let _ = stream.write_all(head.as_bytes());
    if allow_body {
        let _ = stream.write_all(body);
    }
    let _ = stream.flush();
}

fn mime_of(path: &Path) -> &'static str {
    match path.extension().and_then(|e| e.to_str()).unwrap_or("") {
        "html" => "text/html; charset=utf-8",
        "js" => "application/javascript",
        "css" => "text/css",
        "json" => "application/json",
        "md" => "text/markdown; charset=utf-8",
        "png" => "image/png",
        "jpg" | "jpeg" => "image/jpeg",
        "svg" => "image/svg+xml",
        "wasm" => "application/wasm",
        "obj" => "text/plain",
        "mtl" => "text/plain",
        "txt" => "text/plain; charset=utf-8",
        _ => "application/octet-stream",
    }
}

fn query_param(raw: &str, key: &str) -> Option<usize> {
    let q = raw.split('?').nth(1)?;
    for pair in q.split('&') {
        let mut kv = pair.splitn(2, '=');
        if kv.next() == Some(key) {
            return kv.next()?.parse().ok();
        }
    }
    None
}
