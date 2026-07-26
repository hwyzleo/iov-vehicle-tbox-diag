#include "diag_context.h"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <functional>

namespace tbox::diag {

std::string generate_request_id() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    
    std::ostringstream oss;
    oss << "diag-" << timestamp << "-" << std::hex << std::setw(8) << std::setfill('0') << dist(gen);
    return oss.str();
}

std::string generate_trace_id() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    
    std::ostringstream oss;
    oss << "diag-trace-" << timestamp << "-" << std::hex << std::setw(8) << std::setfill('0') << dist(gen);
    return oss.str();
}

std::string hash_source_address(uint16_t source_address) {
    // 使用 std::hash 进行简单哈希，截断到 16 字符
    std::hash<uint16_t> hasher;
    auto hash_value = hasher(source_address);
    
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash_value;
    std::string hash_str = oss.str();
    
    // 截断到 16 字符
    return hash_str.substr(0, 16);
}

} // namespace tbox::diag
