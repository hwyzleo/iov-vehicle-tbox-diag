#pragma once

#include "transport_adapter.h"
#include <string>

namespace tbox {
namespace diag {

struct DoCanConfig {
    std::string can_interface = "can0";
    uint32_t tx_id = 0x7E0;
    uint32_t rx_id = 0x7E8;
    uint32_t connect_timeout_ms = 5000;
};

class DoCanAdapter : public TransportAdapter {
public:
    explicit DoCanAdapter(const DoCanConfig& config);
    ~DoCanAdapter() override = default;

    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;

    bool send(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receive(uint32_t timeout_ms) override;

    TransportType get_transport_type() const override;
    std::string get_name() const override;

private:
    DoCanConfig config_;
    bool connected_ = false;
};

} // namespace diag
} // namespace tbox
