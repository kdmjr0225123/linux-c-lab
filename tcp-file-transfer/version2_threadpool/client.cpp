// Version 2 client: download (default) or upload (-u) with SHA-256 verification.
// Usage:
//   ./client <filename> [output_path] [server_ip]      download
//   ./client -u <local_path> [server_ip]               upload (stored as basename)
#include <libgen.h>

#include <cstdio>
#include <fstream>

#include "../common/net.h"

#define PORT 9000
#define BUFFER_SIZE 4096

static int connect_to(const char* host) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) die("socket");
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, host, &server.sin_addr) != 1) {
        std::cerr << "Invalid server address: " << host << "\n";
        exit(1);
    }
    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) die("connect");
    return sock;
}

static bool send_request(int sock, char cmd, const std::string& name) {
    uint8_t c = uint8_t(cmd);
    return send_all(sock, &c, 1) == 1 && send_u32(sock, uint32_t(name.size())) &&
           send_all(sock, name.data(), name.size()) == ssize_t(name.size());
}

static int do_download(const std::string& filename, const std::string& out_path, const char* host) {
    auto t_start = std::chrono::steady_clock::now();
    int sock = connect_to(host);
    if (!send_request(sock, 'G', filename)) die("send(request)");

    uint8_t status;
    uint64_t file_size;
    uint8_t expected[SHA256::DIGEST_SIZE];
    if (!recv_header(sock, status, file_size, expected)) {
        std::cerr << "Connection closed before response header\n";
        return 1;
    }
    double wait_ms = ms_since(t_start);
    auto t_xfer = std::chrono::steady_clock::now();

    if (status != ST_OK) {
        printf("[pid %d] Server error: %s\n", getpid(), status_str(status));
        printf("RESULT ok=0 status=%s\n", status_str(status));
        close(sock);
        return 1;
    }

    std::ofstream output(out_path, std::ios::binary);
    if (!output) {
        std::cerr << "Cannot open " << out_path << " for writing\n";
        return 1;
    }
    char buffer[BUFFER_SIZE];
    uint64_t received = 0;
    SHA256 sha;
    while (received < file_size) {
        size_t want = size_t(std::min<uint64_t>(BUFFER_SIZE, file_size - received));
        ssize_t bytes = recv(sock, buffer, want, 0);
        if (bytes < 0) {
            if (errno == EINTR) continue;
            die("recv(data)");
        }
        if (bytes == 0) {
            std::cerr << "Server closed connection early (" << received << "/" << file_size << ")\n";
            return 1;
        }
        output.write(buffer, bytes);
        sha.update(buffer, size_t(bytes));
        received += uint64_t(bytes);
    }
    output.close();
    close(sock);

    uint8_t actual[SHA256::DIGEST_SIZE];
    sha.final(actual);
    bool ok = memcmp(actual, expected, SHA256::DIGEST_SIZE) == 0;
    double xfer_ms = ms_since(t_xfer), total_ms = ms_since(t_start);

    printf("[pid %d] %s: %s -> %s | wait %.1f ms | transfer %.1f ms | total %.1f ms | %.1f MiB/s | sha256 %s\n",
           getpid(), ok ? "OK" : "FAIL", filename.c_str(), out_path.c_str(), wait_ms, xfer_ms, total_ms,
           xfer_ms > 0 ? (received / 1048576.0) / (xfer_ms / 1000.0) : 0, ok ? "verified" : "MISMATCH");
    printf("RESULT ok=%d bytes=%llu wait_ms=%.2f xfer_ms=%.2f total_ms=%.2f\n", ok ? 1 : 0,
           (unsigned long long)received, wait_ms, xfer_ms, total_ms);
    if (!ok) { std::remove(out_path.c_str()); return 2; }
    return 0;
}

static int do_upload(const std::string& local_path, const char* host) {
    std::ifstream file(local_path, std::ios::binary | std::ios::ate);
    if (!file) { std::cerr << "Cannot open " << local_path << "\n"; return 1; }
    uint64_t file_size = uint64_t(file.tellg());

    char buffer[BUFFER_SIZE];
    SHA256 sha;
    file.seekg(0);
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) sha.update(buffer, size_t(file.gcount()));
    uint8_t digest[SHA256::DIGEST_SIZE];
    sha.final(digest);

    std::string tmp = local_path;
    std::string name = basename(&tmp[0]);

    auto t_start = std::chrono::steady_clock::now();
    int sock = connect_to(host);
    if (!send_request(sock, 'P', name) || !send_u64(sock, file_size) ||
        send_all(sock, digest, SHA256::DIGEST_SIZE) < 0)
        die("send(upload request)");

    uint8_t status;
    uint64_t sz;
    uint8_t h[SHA256::DIGEST_SIZE];
    if (!recv_header(sock, status, sz, h)) { std::cerr << "No response from server\n"; return 1; }
    double wait_ms = ms_since(t_start);
    if (status != ST_OK) {
        printf("[pid %d] Upload refused: %s\n", getpid(), status_str(status));
        close(sock);
        return 1;
    }

    file.clear();
    file.seekg(0);
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0)
        if (send_all(sock, buffer, size_t(file.gcount())) < 0) die("send(data)");

    if (!recv_header(sock, status, sz, h)) { std::cerr << "No final response from server\n"; return 1; }
    close(sock);
    double total_ms = ms_since(t_start);
    printf("[pid %d] UPLOAD %s: %s (%s) | wait %.1f ms | total %.1f ms\n", getpid(), status_str(status),
           name.c_str(), human_size(file_size).c_str(), wait_ms, total_ms);
    return status == ST_OK ? 0 : 2;
}

int main(int argc, char* argv[]) {
    if (argc >= 3 && std::string(argv[1]) == "-u")
        return do_upload(argv[2], argc >= 4 ? argv[3] : "127.0.0.1");
    if (argc < 2) {
        std::cout << "Usage: ./client <filename> [output_path] [server_ip]\n"
                     "       ./client -u <local_path> [server_ip]\n";
        return 1;
    }
    std::string filename = argv[1];
    return do_download(filename, argc >= 3 ? argv[2] : "received_" + filename,
                       argc >= 4 ? argv[3] : "127.0.0.1");
}
