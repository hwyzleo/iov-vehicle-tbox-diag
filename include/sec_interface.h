#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace tbox {
namespace sec {
enum class ErrorCode : uint32_t;
}
namespace diag {

/// 证书注入失败原因。
///
/// `inject_certificate` 原先只返回 bool，所有失败都被 DIAG 映射成
/// NRC 0x72 generalProgrammingFailure，工位无法区分「状态不对，换个顺序重试」
/// 和「证书本身非法，重新签发」。该枚举把 SEC 的 ErrorCode 收敛成 DIAG 需要
/// 区分的若干类，再映射到不同 NRC。
enum class CertInjectFailure : uint8_t {
    NONE = 0,            ///< 成功
    SEC_UNAVAILABLE,     ///< SEC IPC 不可用
    STATE_NOT_ALLOWED,   ///< provision 状态不允许（顺序错误）
    INVALID_FORMAT,      ///< 链解析/形状非法、证书格式错误
    KEY_MISMATCH,        ///< 证书公钥与设备私钥不匹配
    CERT_EXPIRED,        ///< 证书过期，或可信时间不可用导致无法判定有效期
    STORAGE_FAILED,      ///< 校验通过但原子发布/存储失败
    UNKNOWN,             ///< 其它
};

/// 稳定字符串化（用于日志脱敏字段）。
inline const char* cert_inject_failure_name(CertInjectFailure f) {
    switch (f) {
        case CertInjectFailure::NONE:              return "none";
        case CertInjectFailure::SEC_UNAVAILABLE:   return "sec_unavailable";
        case CertInjectFailure::STATE_NOT_ALLOWED: return "state_not_allowed";
        case CertInjectFailure::INVALID_FORMAT:    return "invalid_format";
        case CertInjectFailure::KEY_MISMATCH:      return "key_mismatch";
        case CertInjectFailure::CERT_EXPIRED:      return "cert_expired";
        case CertInjectFailure::STORAGE_FAILED:    return "storage_failed";
        case CertInjectFailure::UNKNOWN:           return "unknown";
    }
    return "unknown";
}

class SecInterface {
public:
    virtual ~SecInterface() = default;

    // 现有方法
    virtual bool get_seed(uint8_t level, std::vector<uint8_t>& seed) = 0;
    virtual bool verify_key(uint8_t level, const std::vector<uint8_t>& key) = 0;
    virtual bool is_available() const = 0;

    // 新增方法
    virtual bool generate_key_pair() = 0;
    virtual bool get_csr(std::vector<uint8_t>& csr_der) = 0;
    virtual bool submit_csr() = 0;
    virtual bool inject_certificate(const std::vector<uint8_t>& cert_der) = 0;

    /// 带失败原因的证书注入。
    ///
    /// 默认实现回退到 bool 版本并给出 UNKNOWN，使既有实现（含测试 mock / stub）
    /// 无需改动即可继续编译。真实 IPC 适配器覆写此方法以透出 SEC 的错误码。
    virtual bool inject_certificate(const std::vector<uint8_t>& cert_der,
                                    CertInjectFailure& failure) {
        const bool ok = inject_certificate(cert_der);
        failure = ok ? CertInjectFailure::NONE : CertInjectFailure::UNKNOWN;
        return ok;
    }
};

} // namespace diag
} // namespace tbox
