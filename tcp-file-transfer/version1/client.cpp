// Version 1 client: download a file, verify SHA-256, report timing.
// Usage: ./client <filename> [output_path] [server_ip]
#include <cstdio>
#include <fstream>

#include "../common/net.h"

#define PORT 9000
#define BUFFER_SIZE 4096

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./client <filename> [output_path] [server_ip]\n";
        return 1;
    }
    std::string filename = argv[1];
    std::string out_path = argc >= 3 ? argv[2] : "received_" + filename;
    const char* host = argc >= 4 ? argv[3] : "127.0.0.1";

    auto t_start = std::chrono::steady_clock::now();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) die("socket");

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, host, &server.sin_addr) != 1) {
        std::cerr << "Invalid server address: " << host << "\n";
        return 1;
    }
    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) die("connect");

    if (!send_u32(sock, uint32_t(filename.size())) ||
        send_all(sock, filename.c_str(), filename.size()) < 0)
        die("send(request)");

    uint8_t status;
    uint64_t file_size;
    uint8_t expected[SHA256::DIGEST_SIZE];
    if (!recv_header(sock, status, file_size, expected)) {
        std::cerr << "Connection closed before response header\n";
        return 1;
    }
    // Time between connecting and the server starting on our request:
    // this is the time spent blocked behind other clients.
    double wait_ms = ms_since(t_start);
    auto t_xfer = std::chrono::steady_clock::now();

    if (status != ST_OK) {
        std::cout << "Server error: " << status_str(status) << "\n";
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
            std::cerr << "Server closed connection early (" << received << "/" << file_size << " bytes)\n";
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

    double xfer_ms = ms_since(t_xfer);
    double total_ms = ms_since(t_start);
    double mibps = xfer_ms > 0 ? (received / 1048576.0) / (xfer_ms / 1000.0) : 0;

    printf("[pid %d] %s: %s -> %s | wait %.1f ms | transfer %.1f ms | total %.1f ms | %.1f MiB/s | sha256 %s\n",
           getpid(), ok ? "OK" : "FAIL", filename.c_str(), out_path.c_str(), wait_ms, xfer_ms, total_ms,
           mibps, ok ? "verified" : "MISMATCH");
    // Machine-readable line for bench.sh
    printf("RESULT ok=%d bytes=%llu wait_ms=%.2f xfer_ms=%.2f total_ms=%.2f\n", ok ? 1 : 0,
           (unsigned long long)received, wait_ms, xfer_ms, total_ms);

    if (!ok) {
        std::remove(out_path.c_str());
        return 2;
    }
    return 0;
}
