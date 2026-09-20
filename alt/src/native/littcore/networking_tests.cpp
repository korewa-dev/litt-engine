#include "litt_networking.h"
#include <cstdio>
#include <thread>
#include <vector>
using namespace litt::net;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static detail::Socket raw_connect(uint16_t port) {
    detail::ensure_platform();
    detail::Socket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == detail::kInvalidSocket) return s;
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) { detail::close_socket(s); return detail::kInvalidSocket; }
    return s;
}

static void round_trip() {
    TcpServer server; CHECK(server.listen(0) == Result::Ok); const uint16_t port = server.local_port(); CHECK(port != 0);
    std::thread worker([&] {
        TcpPeer peer; CHECK(server.accept(peer) == Result::Ok);
        Message in; CHECK(peer.receive(in) == Result::Ok);
        CHECK(in.id == 7 && in.type == 42 && in.payload == std::vector<uint8_t>({1,2,3,4}));
        CHECK(peer.send(in) == Result::Ok); peer.close();
    });
    TcpClient client; CHECK(client.connect("127.0.0.1", port) == Result::Ok);
    Message out{7, 42, {1,2,3,4}}; CHECK(client.send(out) == Result::Ok);
    Message echo; CHECK(client.receive(echo) == Result::Ok); CHECK(echo.payload == out.payload);
    worker.join(); CHECK(client.receive(echo, 1000) == Result::Disconnected);
}

static void timeout_and_limit() {
    TcpServer server; CHECK(server.listen(0) == Result::Ok);
    TcpClient client; CHECK(client.connect("127.0.0.1", server.local_port()) == Result::Ok);
    TcpPeer peer; CHECK(server.accept(peer) == Result::Ok);
    Message m; CHECK(client.receive(m, 20) == Result::Timeout);
    m.payload.resize(kMaxPayloadBytes + 1u); CHECK(client.send(m) == Result::TooLarge);
}

static void reconnect() {
    TcpServer server; CHECK(server.listen(0) == Result::Ok); const uint16_t port = server.local_port();
    for (int i = 0; i < 2; ++i) {
        TcpClient client; CHECK(client.connect("127.0.0.1", port) == Result::Ok);
        TcpPeer peer; CHECK(server.accept(peer) == Result::Ok);
        client.disconnect(); Message m; CHECK(peer.receive(m, 1000) == Result::Disconnected);
    }
}

static void refused() {
    TcpServer server; CHECK(server.listen(0) == Result::Ok); const uint16_t unused = server.local_port(); server.stop();
    TcpClient client; Result r = client.connect("127.0.0.1", unused);
    CHECK(r == Result::Refused || r == Result::SystemError); CHECK(!client.connected());
}

static void multiple_clients() {
    TcpServer server; CHECK(server.listen(0) == Result::Ok); const uint16_t port = server.local_port();
    TcpClient a, b; TcpPeer pa, pb;
    CHECK(a.connect("127.0.0.1", port) == Result::Ok); CHECK(server.accept(pa) == Result::Ok);
    CHECK(b.connect("127.0.0.1", port) == Result::Ok); CHECK(server.accept(pb) == Result::Ok);
    CHECK(a.send(Message{1, 1, {10}}) == Result::Ok); CHECK(b.send(Message{2, 1, {20}}) == Result::Ok);
    Message ma, mb; CHECK(pa.receive(ma) == Result::Ok && ma.payload[0] == 10);
    CHECK(pb.receive(mb) == Result::Ok && mb.payload[0] == 20);
}

static void malformed_and_partial() {
    {
        TcpServer server; CHECK(server.listen(0) == Result::Ok); detail::Socket raw = raw_connect(server.local_port());
        TcpPeer peer; CHECK(raw != detail::kInvalidSocket); CHECK(server.accept(peer) == Result::Ok);
        detail::WireHeader h{}; h.magic = htonl(0x12345678u); h.version = kWireVersion;
        CHECK(detail::send_all(raw, reinterpret_cast<const uint8_t*>(&h), sizeof(h), 1000) == Result::Ok);
        Message m; CHECK(peer.receive(m) == Result::Malformed); detail::close_socket(raw);
    }
    {
        TcpServer server; CHECK(server.listen(0) == Result::Ok); detail::Socket raw = raw_connect(server.local_port());
        TcpPeer peer; CHECK(raw != detail::kInvalidSocket); CHECK(server.accept(peer) == Result::Ok);
        detail::WireHeader h{htonl(kWireMagic), kWireVersion, 0, htons(3), htonl(4), htonl(9)};
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&h);
        CHECK(detail::send_all(raw, p, 5, 1000) == Result::Ok);
        CHECK(detail::send_all(raw, p + 5, sizeof(h) - 5, 1000) == Result::Ok);
        const uint8_t a[] = {8,7}, b[] = {6,5};
        CHECK(detail::send_all(raw, a, sizeof(a), 1000) == Result::Ok);
        CHECK(detail::send_all(raw, b, sizeof(b), 1000) == Result::Ok);
        Message m; CHECK(peer.receive(m) == Result::Ok);
        CHECK(m.id == 9 && m.type == 3 && m.payload == std::vector<uint8_t>({8,7,6,5})); detail::close_socket(raw);
    }
    {
        TcpServer server; CHECK(server.listen(0) == Result::Ok); detail::Socket raw = raw_connect(server.local_port());
        TcpPeer peer; CHECK(raw != detail::kInvalidSocket); CHECK(server.accept(peer) == Result::Ok);
        detail::WireHeader h{htonl(kWireMagic), kWireVersion, 0, 0, htonl(8), 0};
        CHECK(detail::send_all(raw, reinterpret_cast<const uint8_t*>(&h), sizeof(h), 1000) == Result::Ok);
        const uint8_t partial[] = {1,2,3}; CHECK(detail::send_all(raw, partial, sizeof(partial), 1000) == Result::Ok);
        detail::close_socket(raw); Message m; CHECK(peer.receive(m) == Result::Disconnected);
    }
}

int main() {
    round_trip(); timeout_and_limit(); reconnect(); refused(); multiple_clients(); malformed_and_partial();
    std::printf("networking: %s\n", failures ? "FAILED" : "OK"); return failures ? 1 : 0;
}
