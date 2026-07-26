#include "uds_codec.h"
#include "constants.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include <cstring>
#include <iostream>
#include <cstdio>

namespace tbox {
namespace diag {

bool UdsCodec::decode(const std::vector<uint8_t>& raw, DiagRequest& request) {
    if (raw.empty()) {
        return false;
    }

    request.service_id = raw[0];

    switch (request.service_id) {
        case UdsService::DIAGNOSTIC_SESSION_CONTROL:
            if (raw.size() < 2) return false;
            request.sub_function = raw[1];
            break;

        case UdsService::TESTER_PRESENT:
            if (raw.size() < 2) return false;
            request.sub_function = raw[1];
            break;

        case UdsService::SECURITY_ACCESS:
            if (raw.size() < 2) return false;
            request.sub_function = raw[1];
            if (raw.size() > 2) {
                request.payload.assign(raw.begin() + 2, raw.end());
            }
            break;

        case UdsService::READ_DATA_BY_IDENTIFIER:
            if (raw.size() < 3) return false;
            request.did_or_rid = (static_cast<uint16_t>(raw[1]) << 8) | raw[2];
            break;

        case UdsService::ROUTINE_CONTROL:
            {
                // 将 raw bytes 转为 hex 字符串用于日志
                std::string hex_str;
                hex_str.reserve(raw.size() * 3);
                for (auto b : raw) {
                    char buf[4];
                    snprintf(buf, sizeof(buf), "%02x ", b);
                    hex_str += buf;
                }
                DiagLogAdapter::uds_router().debug(
                    "diag.uds.routine_control_raw",
                    "RoutineControl raw decode",
                    {fw::log::Field(events::fields::PAYLOAD_SIZE,
                        fw::log::FieldValue::makeInt(static_cast<int64_t>(raw.size()))),
                     fw::log::Field("raw_hex",
                        fw::log::FieldValue::makeString(hex_str))}
                );
            }
            if (raw.size() < 4) return false;
            request.sub_function = raw[1];
            request.did_or_rid = (static_cast<uint16_t>(raw[2]) << 8) | raw[3];
            if (raw.size() > 4) {
                request.payload.assign(raw.begin() + 4, raw.end());
            }
            break;

        default:
            if (raw.size() > 1) {
                request.sub_function = raw[1];
            }
            if (raw.size() > 2) {
                request.payload.assign(raw.begin() + 2, raw.end());
            }
            break;
    }

    return true;
}

std::vector<uint8_t> UdsCodec::encode(const DiagResponse& response) {
    std::vector<uint8_t> raw;

    if (response.positive) {
        raw.push_back(response.service_id);
        // ReadDataByIdentifier (0x22) positive response has no sub_function byte:
        // ISO 14229: [0x62] [DID(2)] [data...]
        if (response.service_id != static_cast<uint8_t>(UdsService::READ_DATA_BY_IDENTIFIER) + 0x40) {
            raw.push_back(response.sub_function);
        }
        raw.insert(raw.end(), response.payload.begin(), response.payload.end());
    } else {
        raw.push_back(0x7F);
        raw.push_back(response.sub_function);
        raw.push_back(response.nrc);
    }

    return raw;
}

} // namespace diag
} // namespace tbox
