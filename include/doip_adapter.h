#pragma once

#include "transport_adapter.h"
#include <string>
#include <vector>
#include <mutex>

namespace tbox {
namespace diag {

struct DoIpConfig {
    std::string listen_address = "0.0.0.0";
    uint16_t port = 13400;
    uint16_t local_address = 0x0001;
    uint32_t accept_timeout_ms = 1000;
};

class DoIpAdapter : public TransportAdapter {
public:
    explicit DoIpAdapter(const DoIpConfig& config);
    ~DoIpAdapter() override;

    bool start_server();
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;

    bool send(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receive(uint32_t timeout_ms) override;

    TransportType get_transport_type() const override;
    std::string get_name() const override;

private:
    bool doip_handshake();
    std::vector<uint8_t> build_doip_frame(uint16_t payload_type,
                                           const std::vector<uint8_t>& payload);
    bool parse_doip_frame(const std::vector<uint8_t>& raw,
                          uint16_t& payload_type,
                          std::vector<uint8_t>& payload);

    DoIpConfig config_;
    int server_fd_ = -1;
    int client_fd_ = -1;
    bool connected_ = false;
    bool listening_ = false;
    uint16_t remote_address_ = 0x0000;
    mutable std::mutex mutex_;
};

} // namespace diag
} // namespace tbox
