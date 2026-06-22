#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "data_models.h"
#include "error_codes.h"

namespace tbox {
namespace diag {

struct VinReadResult {
    std::string vin;
    std::string bind_state;
    bool valid = false;
};

class ProvInterface {
public:
    virtual ~ProvInterface() = default;

    virtual DiagErrorCode write_vin(const std::string& vin, const std::vector<uint8_t>& payload) = 0;
    virtual VinReadResult read_vin() = 0;
    virtual bool is_available() const = 0;
};

} // namespace diag
} // namespace tbox
