#pragma once

#include "constants.h"
#include "error_codes.h"
#include <cstdint>
#include <string>

namespace tbox {
namespace diag {

class NrcMapper {
public:
    static uint8_t diag_error_to_nrc(DiagErrorCode error);
    static DiagErrorCode prov_error_to_diag(uint32_t prov_error_code);
    static std::string get_nrc_description(uint8_t nrc);
    static void log_diag_error(DiagErrorCode error, const std::string& context);
};

} // namespace diag
} // namespace tbox
