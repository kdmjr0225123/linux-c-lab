// Version 1: single-threaded (sequential) file server.
// One client at a time; the accept loop blocks while a transfer runs.
#include <csignal>
#include <fstream>

#include "../common/net.h"

#define PORT 9000
#define BUFFER_SIZE 4096
#define BACKLOG 5

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

static uint64_t g_served = 0;
static double g_total_service_ms = 0;

// Request:  [name_len : uint32][filename]
// Response: [status : uint8][file_size : uint64][sha256 : 32][file_data]
void send_file(int client_socket) {
    auto t0 = std::chrono::steady_clock::now();
    std::string ip = peer_ip(client_socket);

    uint32_t name_len;
    if (!recv_u32(client_socket, name_len)) {
        log_msg("ERROR " + ip + " recv(name_len): " + strerror(errno));
        return;
    }
    if (name_len == 0 || name_len > MAX_NAME_LEN) {
        log_msg("REJECT " + ip + " invalid name_len=" + std::to_string(name_len));
        send_header(client_socket, ST_BAD_NAME, 0, nullptr);
        return;
    }

    std::string filename(name_len, '\0');
    if (recv_all(client_socket, &filename[0], name_len) != ssize_t(name_len)) {
        log_msg("ERROR " + ip + " recv(filename): " + strerror(errno));
        return;
    }
    if (!safe_filename(filename)) {
        log_msg("REJECT " + ip + " unsafe filename \"" + filename + "\"");
        send_header(client_socket, ST_BAD_NAME, 0, nullptr);
        return;
    }

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        log_msg("REQUEST " + ip + " file=\"" + filename + "\" -> NOT FOUND");
        send_header(client_socket, ST_NOT_FOUND, 0, nullptr);
        return;
    }
    uint64_t file_size = uint64_t(file.tellg());

    // Pass 1: SHA-256 of the file.
    char buffer[BUFFER_SIZE];
    SHA256 sha;
    file.seekg(0);
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0)
        sha.update(buffer, size_t(file.gcount()));
    uint8_t digest[SHA256::DIGEST_SIZE];
    sha.final(digest);

    log_msg("REQUEST " + ip + " file=\"" + filename + "\" size=" + std::to_string(file_size) +
            " (" + human_size(file_size) + ") sha256=" + SHA256::to_hex(digest).substr(0, 16) + "...");

    if (!send_header(client_socket, ST_OK, file_size, digest)) {
        log_msg("ERROR " + ip + " send(header): " + strerror(errno));
        return;
    }

    // Pass 2: stream the data.
    file.clear();
    file.seekg(0);
    uint64_t sent = 0;
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        size_t n = size_t(file.gcount());
        if (send_all(client_socket, buffer, n) < 0) {
            log_msg("ERROR " + ip + " send(data) after " + std::to_string(sent) + " bytes: " + strerror(errno));
            return;
        }
        sent += n;
    }

    double ms = ms_since(t0);
    g_served++;
    g_total_service_ms += ms;
    char line[160];
    snprintf(line, sizeof(line), "DONE    %s sent %llu bytes in %.1f ms (%.1f MiB/s)", ip.c_str(),
             (unsigned long long)sent, ms, ms > 0 ? (sent / 1048576.0) / (ms / 1000.0) : 0.0);
    log_msg(line);
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa{};
    sa.sa_handler = on_sigint;  // no SA_RESTART: lets Ctrl+C interrupt accept()
    sigaction(SIGINT, &sa, nullptr);

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

    log_msg("Sequential server listening on port " + std::to_string(PORT) + " (pid " +
            std::to_string(getpid()) + ")");

    while (!g_stop) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_socket = accept(server_fd, (sockaddr*)&client_addr, &len);
        if (client_socket < 0) {
            if (errno == EINTR) continue;
            log_msg(std::string("ERROR accept: ") + strerror(errno));
            continue;
        }
        log_msg("ACCEPT  " + peer_ip(client_socket));
        send_file(client_socket);
        if (close(client_socket) < 0) log_msg(std::string("ERROR close: ") + strerror(errno));
    }

    close(server_fd);
    char s[128];
    snprintf(s, sizeof(s), "Shutting down. served=%llu avg_service=%.1f ms", (unsigned long long)g_served,
             g_served ? g_total_service_ms / g_served : 0.0);
    log_msg(s);
    return 0;
}
