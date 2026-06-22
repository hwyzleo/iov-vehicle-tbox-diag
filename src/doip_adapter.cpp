#include "doip_adapter.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <algorithm>

namespace tbox {
namespace diag {

DoIpAdapter::DoIpAdapter(const DoIpConfig& config) : config_(config) {}

DoIpAdapter::~DoIpAdapter() {
    disconnect();
    if (server_fd_ >= 0) {
        ::close(server_fd_);
    }
}

bool DoIpAdapter::start_server() {
    std::lock_guard<std::mutex> lock(mutex_);

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[DoIP] Failed to create socket" << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.listen_address.c_str(), &addr.sin_addr);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[DoIP] Failed to bind to " << config_.listen_address
                  << ":" << config_.port << std::endl;
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 1) < 0) {
        std::cerr << "[DoIP] Failed to listen" << std::endl;
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    listening_ = true;
    std::cout << "[DoIP] Server listening on " << config_.listen_address
              << ":" << config_.port << std::endl;
    return true;
}

bool DoIpAdapter::connect() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connected_) {
        return true;
    }

    if (!listening_) {
        std::cerr << "[DoIP] Server not started" << std::endl;
        return false;
    }

    struct pollfd pfd;
    pfd.fd = server_fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, config_.accept_timeout_ms);
    if (ret <= 0) {
        return false;  // Timeout or error
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    client_fd_ = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd_ < 0) {
        std::cerr << "[DoIP] Failed to accept connection" << std::endl;
        return false;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    std::cout << "[DoIP] Client connected from " << ip_str
              << ":" << ntohs(client_addr.sin_port) << std::endl;

    connected_ = true;

    if (!doip_handshake()) {
        std::cerr << "[DoIP] Handshake failed" << std::endl;
        disconnect();
        return false;
    }

    return true;
}

void DoIpAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (client_fd_ >= 0) {
        ::close(client_fd_);
        client_fd_ = -1;
    }
    connected_ = false;
    std::cout << "[DoIP] Client disconnected" << std::endl;
}

bool DoIpAdapter::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

bool DoIpAdapter::send(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_ || client_fd_ < 0) {
        return false;
    }

    // Prepend DoIP addressing: 2-byte source (TBOX) + 2-byte target (DTE)
    std::vector<uint8_t> doip_payload(4 + data.size());
    doip_payload[0] = static_cast<uint8_t>((config_.local_address >> 8) & 0xFF);
    doip_payload[1] = static_cast<uint8_t>(config_.local_address & 0xFF);
    doip_payload[2] = static_cast<uint8_t>((remote_address_ >> 8) & 0xFF);
    doip_payload[3] = static_cast<uint8_t>(remote_address_ & 0xFF);
    std::copy(data.begin(), data.end(), doip_payload.begin() + 4);

    auto frame = build_doip_frame(0x8001, doip_payload);
    ssize_t sent = ::send(client_fd_, frame.data(), frame.size(), 0);
    return sent == static_cast<ssize_t>(frame.size());
}

std::vector<uint8_t> DoIpAdapter::receive(uint32_t timeout_ms) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_ || client_fd_ < 0) {
            return {};
        }
    }

    struct pollfd pfd;
    pfd.fd = client_fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return {};
    }

    uint8_t header[8];
    ssize_t n = recv(client_fd_, header, 8, MSG_WAITALL);
    if (n != 8) {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = false;
        return {};
    }

    uint32_t payload_len = (static_cast<uint32_t>(header[4]) << 24) |
                           (static_cast<uint32_t>(header[5]) << 16) |
                           (static_cast<uint32_t>(header[6]) << 8) |
                           static_cast<uint32_t>(header[7]);

    if (payload_len > 4096) {
        std::cerr << "[DoIP] Payload too large: " << payload_len << std::endl;
        return {};
    }

    uint16_t payload_type = (static_cast<uint16_t>(header[2]) << 8) | header[3];

    if (payload_type == 0x0005) {
        // Routing activation request → send standard 13-byte response
        // ISO 13400-2: [tester_addr(2)] [entity_addr(2)] [resp_code(1)] [reserved(4)] [oem(4)]
        std::vector<uint8_t> req_payload(payload_len);
        n = recv(client_fd_, req_payload.data(), payload_len, MSG_WAITALL);
        if (n != static_cast<ssize_t>(payload_len)) {
            return {};
        }
        uint16_t tester_addr = 0x0000;
        if (payload_len >= 2) {
            tester_addr = (static_cast<uint16_t>(req_payload[0]) << 8) | req_payload[1];
        }
        std::vector<uint8_t> resp_payload(13, 0);
        resp_payload[0] = static_cast<uint8_t>((tester_addr >> 8) & 0xFF);
        resp_payload[1] = static_cast<uint8_t>(tester_addr & 0xFF);
        resp_payload[2] = static_cast<uint8_t>((config_.local_address >> 8) & 0xFF);
        resp_payload[3] = static_cast<uint8_t>(config_.local_address & 0xFF);
        resp_payload[4] = 0x10;  // routing activation accepted
        auto resp_frame = build_doip_frame(0x0006, resp_payload);
        ::send(client_fd_, resp_frame.data(), resp_frame.size(), 0);
        return {};
    }

    if (payload_type != 0x8001) {
        return {};
    }

    std::vector<uint8_t> payload(payload_len);
    n = recv(client_fd_, payload.data(), payload_len, MSG_WAITALL);
    if (n != static_cast<ssize_t>(payload_len)) {
        return {};
    }

    // Strip DoIP addressing (2-byte source + 2-byte target = 4 bytes)
    if (payload.size() > 4) {
        uint16_t src_addr = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
        uint16_t tgt_addr = (static_cast<uint16_t>(payload[2]) << 8) | payload[3];
        {
            std::lock_guard<std::mutex> lock(mutex_);
            remote_address_ = src_addr;
        }

        // Send DiagnosticMessagePositiveAck (0x8002) before response
        // ISO 13400-2: [source=ECU(2)] [target=Tester(2)] [ack_code(1)]
        std::vector<uint8_t> ack_payload(5, 0);
        ack_payload[0] = static_cast<uint8_t>((config_.local_address >> 8) & 0xFF);
        ack_payload[1] = static_cast<uint8_t>(config_.local_address & 0xFF);
        ack_payload[2] = static_cast<uint8_t>((src_addr >> 8) & 0xFF);
        ack_payload[3] = static_cast<uint8_t>(src_addr & 0xFF);
        ack_payload[4] = 0x00;  // positive ACK
        auto ack_frame = build_doip_frame(0x8002, ack_payload);
        ::send(client_fd_, ack_frame.data(), ack_frame.size(), 0);

        std::vector<uint8_t> uds_data(payload.begin() + 4, payload.end());
        return uds_data;
    }

    return payload;
}

TransportType DoIpAdapter::get_transport_type() const {
    return TransportType::DOIP;
}

std::string DoIpAdapter::get_name() const {
    return "DoIP(" + config_.listen_address + ":" + std::to_string(config_.port) + ")";
}

bool DoIpAdapter::doip_handshake() {
    // Wait for routing activation request from client
    struct pollfd pfd;
    pfd.fd = client_fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, 3000);
    if (ret <= 0) {
        std::cerr << "[DoIP] Handshake timeout" << std::endl;
        return false;
    }

    uint8_t header[8];
    ssize_t n = recv(client_fd_, header, 8, MSG_WAITALL);
    if (n != 8) {
        return false;
    }

    uint16_t payload_type = (static_cast<uint16_t>(header[2]) << 8) | header[3];
    uint32_t payload_len = (static_cast<uint32_t>(header[4]) << 24) |
                           (static_cast<uint32_t>(header[5]) << 16) |
                           (static_cast<uint32_t>(header[6]) << 8) |
                           static_cast<uint32_t>(header[7]);

    if (payload_len > 0) {
        std::vector<uint8_t> buf(payload_len);
        recv(client_fd_, buf.data(), payload_len, MSG_WAITALL);

        if (payload_type == 0x0005) {
            uint16_t tester_addr = 0x0000;
            if (payload_len >= 2) {
                tester_addr = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
            }
            // ISO 13400: [tester_addr(2)] [entity_addr(2)] [resp_code(1)] [reserved(4)] [oem(4)]
            std::vector<uint8_t> resp(13, 0);
            resp[0] = static_cast<uint8_t>((tester_addr >> 8) & 0xFF);
            resp[1] = static_cast<uint8_t>(tester_addr & 0xFF);
            resp[2] = static_cast<uint8_t>((config_.local_address >> 8) & 0xFF);
            resp[3] = static_cast<uint8_t>(config_.local_address & 0xFF);
            resp[4] = 0x10;
            auto frame = build_doip_frame(0x0006, resp);
            ::send(client_fd_, frame.data(), frame.size(), 0);
            std::cout << "[DoIP] Routing activation accepted" << std::endl;
            return true;
        }
    }

    if (payload_type == 0x0005) {
        // No payload in request, send minimal response
        uint16_t tester_addr = 0x0000;
        std::vector<uint8_t> resp(13, 0);
        resp[0] = static_cast<uint8_t>((tester_addr >> 8) & 0xFF);
        resp[1] = static_cast<uint8_t>(tester_addr & 0xFF);
        resp[2] = static_cast<uint8_t>((config_.local_address >> 8) & 0xFF);
        resp[3] = static_cast<uint8_t>(config_.local_address & 0xFF);
        resp[4] = 0x10;
        auto frame = build_doip_frame(0x0006, resp);
        ::send(client_fd_, frame.data(), frame.size(), 0);
        std::cout << "[DoIP] Routing activation accepted" << std::endl;
        return true;
    }

    std::cerr << "[DoIP] Unexpected payload type: 0x" << std::hex << payload_type << std::endl;
    return false;
}

std::vector<uint8_t> DoIpAdapter::build_doip_frame(uint16_t payload_type,
                                                     const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame(8 + payload.size());
    frame[0] = 0x02;  // DoIP version
    frame[1] = 0xFD;  // inverse
    frame[2] = static_cast<uint8_t>((payload_type >> 8) & 0xFF);
    frame[3] = static_cast<uint8_t>(payload_type & 0xFF);
    uint32_t len = static_cast<uint32_t>(payload.size());
    frame[4] = static_cast<uint8_t>((len >> 24) & 0xFF);
    frame[5] = static_cast<uint8_t>((len >> 16) & 0xFF);
    frame[6] = static_cast<uint8_t>((len >> 8) & 0xFF);
    frame[7] = static_cast<uint8_t>(len & 0xFF);
    std::copy(payload.begin(), payload.end(), frame.begin() + 8);
    return frame;
}

bool DoIpAdapter::parse_doip_frame(const std::vector<uint8_t>& raw,
                                    uint16_t& payload_type,
                                    std::vector<uint8_t>& payload) {
    if (raw.size() < 8) return false;
    payload_type = (static_cast<uint16_t>(raw[2]) << 8) | raw[3];
    uint32_t len = (static_cast<uint32_t>(raw[4]) << 24) |
                   (static_cast<uint32_t>(raw[5]) << 16) |
                   (static_cast<uint32_t>(raw[6]) << 8) |
                   static_cast<uint32_t>(raw[7]);
    if (raw.size() < 8 + len) return false;
    payload.assign(raw.begin() + 8, raw.begin() + 8 + len);
    return true;
}

} // namespace diag
} // namespace tbox
