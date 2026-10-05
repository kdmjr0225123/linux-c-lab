// Version 2: thread pool file server (download + upload).
// Usage: ./server [threads]   (default 4)
//
// Request (client -> server):
//   [cmd : uint8 'G' or 'P'][name_len : uint32][filename]
//   cmd 'P' (upload) continues with [file_size : uint64][sha256 : 32]
//
// GET:  server -> [header: OK/err, size, sha256][file_data]
// PUT:  server -> [header: OK = ready, or error]
//       client -> [file_data]
//       server -> [header: OK = stored, CHECKSUM_FAIL, IO_ERROR]
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#include <csignal>
#include <cstdio>
#include <fstream>

#include "../common/net.h"
#include "threadpool.h"

#define PORT 9000
#define BUFFER_SIZE 4096
#define THREADS 4
#define BACKLOG 64

// Challenge 5: resource limits
#define MAX_FILE_SIZE (1ull << 30)   // 1 GiB per download/upload
#define MAX_CONNECTIONS 16           // queued + in service
#define IDLE_TIMEOUT_SEC 10          // no bytes for this long -> drop client

#define UPLOAD_DIR "uploads"

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }
static std::atomic<uint64_t> g_upload_seq{0};

static void handle_download(int sock, const std::string& ip, const std::string& filename) {
    auto t0 = std::chrono::steady_clock::now();
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        log_msg("GET     " + ip + " file=\"" + filename + "\" -> NOT FOUND");
        send_header(sock, ST_NOT_FOUND, 0, nullptr);
        return;
    }
    uint64_t file_size = uint64_t(file.tellg());
    if (file_size > MAX_FILE_SIZE) {
        log_msg("GET     " + ip + " file=\"" + filename + "\" size=" + human_size(file_size) + " -> TOO LARGE");
        send_header(sock, ST_TOO_LARGE, file_size, nullptr);
        return;
    }

    char buffer[BUFFER_SIZE];
    SHA256 sha;
    file.seekg(0);
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0)
        sha.update(buffer, size_t(file.gcount()));
    uint8_t digest[SHA256::DIGEST_SIZE];
    sha.final(digest);

    log_msg("GET     " + ip + " file=\"" + filename + "\" size=" + std::to_string(file_size) + " (" +
            human_size(file_size) + ") sha256=" + SHA256::to_hex(digest).substr(0, 16) + "...");

    if (!send_header(sock, ST_OK, file_size, digest)) {
        log_msg("ERROR   " + ip + " send(header): " + strerror(errno));
        return;
    }

    file.clear();
    file.seekg(0);
    uint64_t sent = 0;
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        size_t n = size_t(file.gcount());
        if (send_all(sock, buffer, n) < 0) {
            log_msg("ERROR   " + ip + " send(data) after " + std::to_string(sent) + " bytes: " +
                    (errno == EAGAIN || errno == EWOULDBLOCK ? "idle timeout" : strerror(errno)));
            return;
        }
        sent += n;
    }
    double ms = ms_since(t0);
    char line[160];
    snprintf(line, sizeof(line), "DONE    %s GET %llu bytes in %.1f ms (%.1f MiB/s)", ip.c_str(),
             (unsigned long long)sent, ms, ms > 0 ? (sent / 1048576.0) / (ms / 1000.0) : 0.0);
    log_msg(line);
}

static void handle_upload(int sock, const std::string& ip, const std::string& filename) {
    auto t0 = std::chrono::steady_clock::now();
    uint64_t file_size;
    uint8_t expected[SHA256::DIGEST_SIZE];
    if (!recv_u64(sock, file_size) ||
        recv_all(sock, expected, SHA256::DIGEST_SIZE) != ssize_t(SHA256::DIGEST_SIZE)) {
        log_msg("ERROR   " + ip + " recv(upload header) failed");
        return;
    }
    if (file_size > MAX_FILE_SIZE) {
        log_msg("PUT     " + ip + " file=\"" + filename + "\" size=" + human_size(file_size) + " -> TOO LARGE");
        send_header(sock, ST_TOO_LARGE, 0, nullptr);
        return;
    }

    // Store safely: write to a unique temp file, verify, then rename().
    // rename() is atomic, so concurrent uploads of the same name never
    // interleave bytes and readers never see a half-written file.
    std::string final_path = std::string(UPLOAD_DIR) + "/" + filename;
    std::string tmp_path = std::string(UPLOAD_DIR) + "/.tmp_" + std::to_string(getpid()) + "_" +
                           std::to_string(g_upload_seq.fetch_add(1)) + "_" + filename;
    int fd = open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        log_msg("ERROR   " + ip + " open(" + tmp_path + "): " + strerror(errno));
        send_header(sock, ST_IO_ERROR, 0, nullptr);
        return;
    }

    log_msg("PUT     " + ip + " file=\"" + filename + "\" size=" + std::to_string(file_size) + " (" +
            human_size(file_size) + ")");
    if (!send_header(sock, ST_OK, 0, nullptr)) {  // "ready"
        close(fd);
        unlink(tmp_path.c_str());
        return;
    }

    char buffer[BUFFER_SIZE];
    SHA256 sha;
    uint64_t received = 0;
    bool failed = false;
    while (received < file_size) {
        size_t want = size_t(std::min<uint64_t>(BUFFER_SIZE, file_size - received));
        ssize_t n = recv(sock, buffer, want, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            log_msg("ERROR   " + ip + " upload aborted at " + std::to_string(received) + "/" +
                    std::to_string(file_size) + " bytes: " +
                    (n == 0 ? "peer closed" : (errno == EAGAIN || errno == EWOULDBLOCK) ? "idle timeout" : strerror(errno)));
            failed = true;
            break;
        }
        if (write(fd, buffer, size_t(n)) != n) {
            log_msg("ERROR   " + ip + " write: " + strerror(errno));
            failed = true;
            break;
        }
        sha.update(buffer, size_t(n));
        received += uint64_t(n);
    }
    if (!failed && fsync(fd) < 0) failed = true;
    close(fd);

    if (failed) {
        unlink(tmp_path.c_str());
        send_header(sock, ST_IO_ERROR, received, nullptr);
        return;
    }

    uint8_t actual[SHA256::DIGEST_SIZE];
    sha.final(actual);
    if (memcmp(actual, expected, SHA256::DIGEST_SIZE) != 0) {
        unlink(tmp_path.c_str());
        log_msg("PUT     " + ip + " file=\"" + filename + "\" -> CHECKSUM MISMATCH, discarded");
        send_header(sock, ST_CHECKSUM_FAIL, received, actual);
        return;
    }
    if (rename(tmp_path.c_str(), final_path.c_str()) < 0) {
        unlink(tmp_path.c_str());
        send_header(sock, ST_IO_ERROR, received, nullptr);
        return;
    }
    send_header(sock, ST_OK, received, actual);
    double ms = ms_since(t0);
    char line[200];
    snprintf(line, sizeof(line), "DONE    %s PUT %llu bytes -> %s in %.1f ms (sha256 verified)", ip.c_str(),
             (unsigned long long)received, final_path.c_str(), ms);
    log_msg(line);
}

void handle_client(int client_socket) {
    std::string ip = peer_ip(client_socket);

    uint8_t cmd;
    uint32_t name_len;
    if (recv_all(client_socket, &cmd, 1) != 1 || !recv_u32(client_socket, name_len)) {
        log_msg("ERROR   " + ip + " recv(request): " +
                ((errno == EAGAIN || errno == EWOULDBLOCK) ? "idle timeout" : "client closed / bad request"));
        return;
    }
    if (name_len == 0 || name_len > MAX_NAME_LEN) {
        log_msg("REJECT  " + ip + " invalid name_len=" + std::to_string(name_len));
        send_header(client_socket, ST_BAD_NAME, 0, nullptr);
        return;
    }
    std::string filename(name_len, '\0');
    if (recv_all(client_socket, &filename[0], name_len) != ssize_t(name_len)) {
        log_msg("ERROR   " + ip + " recv(filename) failed");
        return;
    }
    if (!safe_filename(filename)) {
        log_msg("REJECT  " + ip + " unsafe filename \"" + filename + "\"");
        send_header(client_socket, ST_BAD_NAME, 0, nullptr);
        return;
    }

    if (cmd == 'G') handle_download(client_socket, ip, filename);
    else if (cmd == 'P') handle_upload(client_socket, ip, filename);
    else {
        log_msg("REJECT  " + ip + " unknown command");
        send_header(client_socket, ST_BAD_NAME, 0, nullptr);
    }
}

// Tell an over-limit client we're busy, then close without hanging the accept loop.
static void reject_busy(int sock) {
    send_header(sock, ST_BUSY, 0, nullptr);
    shutdown(sock, SHUT_WR);
    char drain[512];
    while (recv(sock, drain, sizeof(drain), MSG_DONTWAIT) > 0) {}  // avoid RST eating our reply
    close(sock);
}

int main(int argc, char* argv[]) {
    size_t nthreads = argc >= 2 ? size_t(std::max(1, atoi(argv[1]))) : THREADS;

    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigaction(SIGINT, &sa, nullptr);

    if (mkdir(UPLOAD_DIR, 0755) < 0 && errno != EEXIST) die("mkdir uploads");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) die("socket");
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) die("setsockopt");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) die("bind");
    if (listen(server_fd, BACKLOG) < 0) die("listen");

    // Workers inherit a mask with SIGINT blocked so Ctrl+C always lands on the
    // main thread and interrupts accept().
    sigset_t set, old;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, &old);
    {
        ThreadPool pool(nthreads);
        pthread_sigmask(SIG_SETMASK, &old, nullptr);

        log_msg("Thread pool server running on port " + std::to_string(PORT) + " with " +
                std::to_string(nthreads) + " workers (pid " + std::to_string(getpid()) +
                ", max_conn=" + std::to_string(MAX_CONNECTIONS) + ", idle_timeout=" +
                std::to_string(IDLE_TIMEOUT_SEC) + "s, max_file=" + human_size(MAX_FILE_SIZE) + ")");

        while (!g_stop) {
            int client_socket = accept(server_fd, nullptr, nullptr);
            if (client_socket < 0) {
                if (errno == EINTR) continue;
                log_msg(std::string("ERROR accept: ") + strerror(errno));
                continue;
            }

            if (pool.pending() >= MAX_CONNECTIONS) {
                log_msg("REJECT  " + peer_ip(client_socket) + " server busy (" +
                        std::to_string(pool.pending()) + " connections)");
                reject_busy(client_socket);
                continue;
            }

            timeval tv{IDLE_TIMEOUT_SEC, 0};
            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            log_msg("ACCEPT  " + peer_ip(client_socket));
            pool.enqueue(client_socket);
        }
        log_msg("Shutting down: draining queue...");
        char s[160];
        snprintf(s, sizeof(s), "FINAL   served=%llu avg_service=%.1fms avg_queue_wait=%.1fms",
                 (unsigned long long)pool.served(), pool.avg_service_ms(), pool.avg_queue_wait_ms());
        log_msg(s);
    }  // pool destructor joins workers

    close(server_fd);
    return 0;
}
