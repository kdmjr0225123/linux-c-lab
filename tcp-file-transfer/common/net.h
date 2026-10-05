// Shared socket helpers, protocol definitions, and logging for both versions.
#ifndef NET_H
#define NET_H

#include <arpa/inet.h>
#include <endian.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "sha256.h"

// ---------------- Protocol ----------------
// All integers in network byte order.
//
// Response header (server -> client), always 41 bytes:
//   [status : uint8][file_size : uint64][sha256 : 32 bytes]
//
// Challenge 1: file_size is uint64_t (uint32_t caps files at 4 GiB).
// A status byte replaces the starter's "size 0 means not found" convention,
// so empty files and errors are no longer ambiguous.

enum Status : uint8_t {
    ST_OK = 0,
    ST_NOT_FOUND = 1,
    ST_TOO_LARGE = 2,
    ST_BAD_NAME = 3,
    ST_BUSY = 4,
    ST_CHECKSUM_FAIL = 5,
    ST_IO_ERROR = 6,
};

inline const char* status_str(uint8_t s) {
    switch (s) {
        case ST_OK: return "OK";
        case ST_NOT_FOUND: return "NOT_FOUND";
        case ST_TOO_LARGE: return "TOO_LARGE";
        case ST_BAD_NAME: return "BAD_NAME";
        case ST_BUSY: return "SERVER_BUSY";
        case ST_CHECKSUM_FAIL: return "CHECKSUM_FAIL";
        case ST_IO_ERROR: return "IO_ERROR";
        default: return "UNKNOWN";
    }
}

const size_t HEADER_SIZE = 1 + 8 + SHA256::DIGEST_SIZE;
const uint32_t MAX_NAME_LEN = 255;

// ---------------- Logging ----------------
inline std::mutex& log_mutex() { static std::mutex m; return m; }

inline void log_msg(const std::string& msg) {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    int ms = int(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
    char ts[32];
    std::tm tmv;
    localtime_r(&t, &tmv);
    strftime(ts, sizeof(ts), "%H:%M:%S", &tmv);
    std::ostringstream tid;
    tid << std::this_thread::get_id();
    std::lock_guard<std::mutex> lk(log_mutex());
    std::cout << "[" << ts << "." << (ms < 100 ? (ms < 10 ? "00" : "0") : "") << ms
              << "][tid " << tid.str().substr(tid.str().size() > 5 ? tid.str().size() - 5 : 0)
              << "] " << msg << std::endl;
}

inline void die(const char* what) {
    std::cerr << "FATAL: " << what << ": " << strerror(errno) << std::endl;
    exit(1);
}

// ---------------- Challenge 2: partial send()/recv() ----------------
// send() may write fewer bytes than asked (socket buffer full, signal);
// recv() returns whatever has arrived so far. Loop until all bytes move.

// Returns length on success, -1 on error.
inline ssize_t send_all(int sock, const void* buffer, size_t length) {
    const char* p = static_cast<const char*>(buffer);
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(sock, p + sent, length - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += size_t(n);
    }
    return ssize_t(sent);
}

// Returns length on success, number of bytes read (< length) if the peer
// closed early, or -1 on error (including SO_RCVTIMEO idle timeout).
inline ssize_t recv_all(int sock, void* buffer, size_t length) {
    char* p = static_cast<char*>(buffer);
    size_t got = 0;
    while (got < length) {
        ssize_t n = recv(sock, p + got, length - got, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;  // peer closed
        got += size_t(n);
    }
    return ssize_t(got);
}

inline bool send_u32(int s, uint32_t v) { v = htonl(v); return send_all(s, &v, 4) == 4; }
inline bool recv_u32(int s, uint32_t& v) {
    if (recv_all(s, &v, 4) != 4) return false;
    v = ntohl(v); return true;
}
inline bool send_u64(int s, uint64_t v) { v = htobe64(v); return send_all(s, &v, 8) == 8; }
inline bool recv_u64(int s, uint64_t& v) {
    if (recv_all(s, &v, 8) != 8) return false;
    v = be64toh(v); return true;
}

inline bool send_header(int s, uint8_t status, uint64_t size, const uint8_t* hash) {
    uint8_t hdr[HEADER_SIZE] = {0};
    hdr[0] = status;
    uint64_t be = htobe64(size);
    memcpy(hdr + 1, &be, 8);
    if (hash) memcpy(hdr + 9, hash, SHA256::DIGEST_SIZE);
    return send_all(s, hdr, HEADER_SIZE) == ssize_t(HEADER_SIZE);
}

inline bool recv_header(int s, uint8_t& status, uint64_t& size, uint8_t* hash) {
    uint8_t hdr[HEADER_SIZE];
    if (recv_all(s, hdr, HEADER_SIZE) != ssize_t(HEADER_SIZE)) return false;
    status = hdr[0];
    uint64_t be;
    memcpy(&be, hdr + 1, 8);
    size = be64toh(be);
    memcpy(hash, hdr + 9, SHA256::DIGEST_SIZE);
    return true;
}

// ---------------- Misc ----------------
inline std::string peer_ip(int sock) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(sock, (sockaddr*)&addr, &len) < 0) return "unknown";
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

// Reject path traversal and hidden/odd names: only a plain file in the cwd.
inline bool safe_filename(const std::string& n) {
    if (n.empty() || n.size() > MAX_NAME_LEN) return false;
    if (n[0] == '.') return false;
    for (char c : n)
        if (c == '/' || c == '\\' || c == '\0') return false;
    return true;
}

inline double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

inline std::string human_size(uint64_t b) {
    char s[32];
    if (b >= (1ull << 30)) snprintf(s, sizeof(s), "%.2f GiB", b / double(1ull << 30));
    else if (b >= (1ull << 20)) snprintf(s, sizeof(s), "%.2f MiB", b / double(1ull << 20));
    else if (b >= 1024) snprintf(s, sizeof(s), "%.2f KiB", b / 1024.0);
    else snprintf(s, sizeof(s), "%llu B", (unsigned long long)b);
    return s;
}

#endif
