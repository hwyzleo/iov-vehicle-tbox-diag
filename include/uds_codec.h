#pragma once

#include "data_models.h"
#include <vector>
#include <cstdint>

namespace tbox {
namespace diag {

class UdsCodec {
public:
    static bool decode(const std::vector<uint8_t>& raw, DiagRequest& request);
    static std::vector<uint8_t> encode(const DiagResponse& response);
};

} // namespace diag
} // namespace tbox
