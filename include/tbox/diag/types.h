#pragma once

#include <string>
#include <cstdint>

namespace tbox {
namespace diag {

/// DIAG 服务状态（通过 IPC 查询）
struct DiagServiceStatus {
    bool initialized = false;
    std::string session_type;       ///< "DEFAULT" / "PROGRAMMING" / "EXTENDED"
    std::string session_state;      ///< "IDLE" / "ACTIVE" / "SECURITY_UNLOCKED" / "TIMED_OUT"
    bool security_unlocked = false;
    std::string transport;          ///< "DOIP" / "DOCAN"
};

/// 车辆信息（通过 DIAG IPC 路由至 PROV）
struct VehicleInfo {
    bool valid = false;
    std::string vin;
    std::string bind_state;         ///< "NONE" / "VIN_WRITTEN" / "BOUND" / "FAILED"
};

} // namespace diag
} // namespace tbox
