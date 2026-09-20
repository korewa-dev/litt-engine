#pragma once

#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace litt { namespace net {

static constexpr uint32_t kWireMagic = 0x4c495454u;
static constexpr uint8_t kWireVersion = 1;
static constexpr uint32_t kMaxPayloadBytes = 64u * 1024u;
static constexpr int kDefaultTimeoutMs = 2000;

enum class Result { Ok, Timeout, Disconnected, Refused, Malformed, TooLarge, InvalidArgument, SystemError };

inline const char* result_string(Result r) {
    switch (r) {
        case Result::Ok: return "ok"; case Result::Timeout: return "timeout";
        case Result::Disconnected: return "disconnected"; case Result::Refused: return "refused";
        case Result::Malformed: return "malformed"; case Result::TooLarge: return "too_large";
        case Result::InvalidArgument: return "invalid_argument"; case Result::SystemError: return "system_error";
    }
    return "unknown";
}

struct Message { uint32_t id = 0; uint16_t type = 0; std::vector<uint8_t> payload; };

namespace detail {
#if defined(_WIN32)
using Socket = SOCKET;
static constexpr Socket kInvalidSocket = INVALID_SOCKET;
inline int last_error() { return WSAGetLastError(); }
inline void close_socket(Socket s) { if (s != kInvalidSocket) closesocket(s); }
inline bool interrupted(int e) { return e == WSAEINTR; }
inline bool transient(int e) { return e == WSAEWOULDBLOCK; }
inline bool refused(int e) { return e == WSAECONNREFUSED; }
#else
using Socket = int;
static constexpr Socket kInvalidSocket = -1;
inline int last_error() { return errno; }
inline void close_socket(Socket s) { if (s != kInvalidSocket) ::close(s); }
inline bool interrupted(int e) { return e == EINTR; }
inline bool transient(int e) { return e == EAGAIN || e == EWOULDBLOCK; }
inline bool refused(int e) { return e == ECONNREFUSED; }
#endif

inline Result ensure_platform() {
#if defined(_WIN32)
    static const bool ok = [] { WSADATA d{}; return WSAStartup(MAKEWORD(2, 2), &d) == 0; }();
    return ok ? Result::Ok : Result::SystemError;
#else
    return Result::Ok;
#endif
}

inline Result wait(Socket s, bool write, int timeout_ms) {
    if (timeout_ms < 0) return Result::InvalidArgument;
    for (;;) {
        fd_set set; FD_ZERO(&set); FD_SET(s, &set);
        timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        int rc = select(static_cast<int>(s) + 1, write ? nullptr : &set, write ? &set : nullptr, nullptr, &tv);
        if (rc > 0) return Result::Ok;
        if (rc == 0) return Result::Timeout;
        if (!interrupted(last_error())) return Result::SystemError;
    }
}

inline Result send_all(Socket s, const uint8_t* data, size_t size, int timeout_ms) {
    for (size_t done = 0; done < size;) {
        Result r = wait(s, true, timeout_ms); if (r != Result::Ok) return r;
        size_t left = size - done;
        int count = static_cast<int>(left > static_cast<size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : left);
#if defined(_WIN32)
        int n = ::send(s, reinterpret_cast<const char*>(data + done), count, 0);
#else
        int n = static_cast<int>(::send(s, data + done, static_cast<size_t>(count), MSG_NOSIGNAL));
#endif
        if (n == 0) return Result::Disconnected;
        if (n < 0) { int e = last_error(); if (interrupted(e) || transient(e)) continue; return Result::SystemError; }
        done += static_cast<size_t>(n);
    }
    return Result::Ok;
}

inline Result recv_all(Socket s, uint8_t* data, size_t size, int timeout_ms) {
    for (size_t done = 0; done < size;) {
        Result r = wait(s, false, timeout_ms); if (r != Result::Ok) return r;
        size_t left = size - done;
        int count = static_cast<int>(left > static_cast<size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : left);
#if defined(_WIN32)
        int n = ::recv(s, reinterpret_cast<char*>(data + done), count, 0);
#else
        int n = static_cast<int>(::recv(s, data + done, static_cast<size_t>(count), 0));
#endif
        if (n == 0) return Result::Disconnected;
        if (n < 0) { int e = last_error(); if (interrupted(e) || transient(e)) continue; return Result::SystemError; }
        done += static_cast<size_t>(n);
    }
    return Result::Ok;
}

#pragma pack(push, 1)
struct WireHeader { uint32_t magic; uint8_t version; uint8_t reserved; uint16_t type; uint32_t payload_size; uint32_t id; };
#pragma pack(pop)
static_assert(sizeof(WireHeader) == 16, "network wire header must stay stable");
}

class TcpPeer {
public:
    TcpPeer() = default;
    ~TcpPeer() { close(); }
    TcpPeer(const TcpPeer&) = delete;
    TcpPeer& operator=(const TcpPeer&) = delete;
    TcpPeer(TcpPeer&& o) noexcept : socket_(o.socket_) { o.socket_ = detail::kInvalidSocket; }
    TcpPeer& operator=(TcpPeer&& o) noexcept { if (this != &o) { close(); socket_ = o.socket_; o.socket_ = detail::kInvalidSocket; } return *this; }

    bool connected() const { return socket_ != detail::kInvalidSocket; }
    void close() {
        if (!connected()) return;
#if defined(_WIN32)
        shutdown(socket_, SD_BOTH);
#else
        shutdown(socket_, SHUT_RDWR);
#endif
        detail::close_socket(socket_); socket_ = detail::kInvalidSocket;
    }

    Result send(const Message& m, int timeout_ms = kDefaultTimeoutMs) {
        if (!connected()) return Result::Disconnected;
        if (m.payload.size() > kMaxPayloadBytes) return Result::TooLarge;
        detail::WireHeader h{htonl(kWireMagic), kWireVersion, 0, htons(m.type), htonl(static_cast<uint32_t>(m.payload.size())), htonl(m.id)};
        Result r = detail::send_all(socket_, reinterpret_cast<const uint8_t*>(&h), sizeof(h), timeout_ms);
        if (r == Result::Ok && !m.payload.empty()) r = detail::send_all(socket_, m.payload.data(), m.payload.size(), timeout_ms);
        if (r == Result::Disconnected || r == Result::SystemError) close();
        return r;
    }

    Result receive(Message& m, int timeout_ms = kDefaultTimeoutMs) {
        if (!connected()) return Result::Disconnected;
        detail::WireHeader h{};
        Result r = detail::recv_all(socket_, reinterpret_cast<uint8_t*>(&h), sizeof(h), timeout_ms);
        if (r != Result::Ok) { if (r == Result::Disconnected || r == Result::SystemError) close(); return r; }
        uint32_t size = ntohl(h.payload_size);
        if (ntohl(h.magic) != kWireMagic || h.version != kWireVersion || h.reserved != 0) { close(); return Result::Malformed; }
        if (size > kMaxPayloadBytes) { close(); return Result::TooLarge; }
        Message incoming; incoming.id = ntohl(h.id); incoming.type = ntohs(h.type); incoming.payload.resize(size);
        if (size) r = detail::recv_all(socket_, incoming.payload.data(), size, timeout_ms);
        if (r != Result::Ok) { if (r == Result::Disconnected || r == Result::SystemError) close(); return r; }
        m = std::move(incoming); return Result::Ok;
    }

private:
    explicit TcpPeer(detail::Socket s) : socket_(s) {}
    detail::Socket socket_ = detail::kInvalidSocket;
    friend class TcpServer; friend class TcpClient;
};

class TcpServer {
public:
    ~TcpServer() { stop(); }
    TcpServer(const TcpServer&) = delete; TcpServer& operator=(const TcpServer&) = delete;
    TcpServer() = default;

    Result listen(uint16_t port, const char* bind_address = "127.0.0.1", int backlog = 8) {
        stop(); if (!bind_address || backlog < 1) return Result::InvalidArgument;
        if (detail::ensure_platform() != Result::Ok) return Result::SystemError;
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP; hints.ai_flags = AI_NUMERICSERV;
        char service[6]; std::snprintf(service, sizeof(service), "%u", static_cast<unsigned>(port));
        addrinfo* list = nullptr; if (getaddrinfo(bind_address, service, &hints, &list) != 0) return Result::InvalidArgument;
        for (addrinfo* it = list; it; it = it->ai_next) {
            detail::Socket s = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol); if (s == detail::kInvalidSocket) continue;
            int reuse = 1;
#if defined(_WIN32)
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
            if (::bind(s, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0 && ::listen(s, backlog) == 0) { socket_ = s; break; }
            detail::close_socket(s);
        }
        freeaddrinfo(list); return listening() ? Result::Ok : Result::SystemError;
    }

    void stop() { detail::close_socket(socket_); socket_ = detail::kInvalidSocket; }
    bool listening() const { return socket_ != detail::kInvalidSocket; }
    uint16_t local_port() const {
        if (!listening()) return 0; sockaddr_storage a{};
#if defined(_WIN32)
        int n = sizeof(a);
#else
        socklen_t n = sizeof(a);
#endif
        if (getsockname(socket_, reinterpret_cast<sockaddr*>(&a), &n) != 0) return 0;
        if (a.ss_family == AF_INET) return ntohs(reinterpret_cast<const sockaddr_in*>(&a)->sin_port);
        if (a.ss_family == AF_INET6) return ntohs(reinterpret_cast<const sockaddr_in6*>(&a)->sin6_port);
        return 0;
    }
    Result accept(TcpPeer& peer, int timeout_ms = kDefaultTimeoutMs) {
        if (!listening()) return Result::InvalidArgument;
        Result r = detail::wait(socket_, false, timeout_ms); if (r != Result::Ok) return r;
        detail::Socket s = ::accept(socket_, nullptr, nullptr); if (s == detail::kInvalidSocket) return Result::SystemError;
        peer = TcpPeer(s); return Result::Ok;
    }
private: detail::Socket socket_ = detail::kInvalidSocket;
};

class TcpClient {
public:
    Result connect(const char* host, uint16_t port) {
        peer_.close(); if (!host || !*host || port == 0) return Result::InvalidArgument;
        if (detail::ensure_platform() != Result::Ok) return Result::SystemError;
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP; hints.ai_flags = AI_NUMERICSERV;
        char service[6]; std::snprintf(service, sizeof(service), "%u", static_cast<unsigned>(port));
        addrinfo* list = nullptr; if (getaddrinfo(host, service, &hints, &list) != 0) return Result::InvalidArgument;
        Result result = Result::SystemError;
        for (addrinfo* it = list; it; it = it->ai_next) {
            detail::Socket s = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol); if (s == detail::kInvalidSocket) continue;
            if (::connect(s, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) { peer_ = TcpPeer(s); result = Result::Ok; break; }
            if (detail::refused(detail::last_error())) result = Result::Refused; detail::close_socket(s);
        }
        freeaddrinfo(list); return result;
    }
    void disconnect() { peer_.close(); }
    bool connected() const { return peer_.connected(); }
    Result send(const Message& m, int timeout_ms = kDefaultTimeoutMs) { return peer_.send(m, timeout_ms); }
    Result receive(Message& m, int timeout_ms = kDefaultTimeoutMs) { return peer_.receive(m, timeout_ms); }
private: TcpPeer peer_;
};

}} // namespace litt::net
