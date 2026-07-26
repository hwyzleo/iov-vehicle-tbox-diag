#pragma once

#include "log.h"
#include "log_types.h"
#include <string>

namespace tbox::diag {

class DiagLogAdapter {
public:
    // 初始化日志系统（在 main.cpp 中调用一次）
    static tbox::fw::log::InitResult init(
        const std::string& service,
        const tbox::fw::log::LogConfig& config
    );

    // 检查是否已初始化
    static bool is_initialized();

    // 获取各模块的 Logger 实例
    static tbox::fw::log::Logger transport();
    static tbox::fw::log::Logger session();
    static tbox::fw::log::Logger uds_router();
    static tbox::fw::log::Logger downstream();
    static tbox::fw::log::Logger response();

private:
    static bool s_initialized;
};

} // namespace tbox::diag
