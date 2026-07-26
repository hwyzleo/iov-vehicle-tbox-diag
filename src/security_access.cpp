#include "security_access.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include <iostream>
#include <chrono>

namespace tbox {
namespace diag {

SecurityAccess::SecurityAccess(std::shared_ptr<SecInterface> sec) : sec_(sec) {}

DiagErrorCode SecurityAccess::request_seed(uint8_t level, std::vector<uint8_t>& seed) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if SEC is available
    if (!sec_ || !sec_->is_available()) {
        return DiagErrorCode::SEC_UNAVAILABLE;
    }

    // Check if already locked out
    if (is_locked_out(level)) {
        return DiagErrorCode::SECURITY_ACCESS_DENIED;
    }

    // Get seed from SEC
    if (!sec_->get_seed(level, seed)) {
        return DiagErrorCode::SEC_UNAVAILABLE;
    }

    // Update state
    auto& state = states_[level];
    state.level = level;
    state.seed_requested = true;
    state.requested_seed = seed;
    state.unlocked = false;

    return DiagErrorCode::SUCCESS;
}

DiagErrorCode SecurityAccess::send_key(uint8_t level, const std::vector<uint8_t>& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if SEC is available
    if (!sec_ || !sec_->is_available()) {
        return DiagErrorCode::SEC_UNAVAILABLE;
    }

    // ISO 14229: sendKey level is even, requestSeed level is odd (sendKey - 1)
    // Normalize to requestSeed level for state lookup
    uint8_t seed_level = (level > 0) ? (level - 1) : level;

    // Check if already locked out
    if (is_locked_out(seed_level)) {
        return DiagErrorCode::SECURITY_ACCESS_DENIED;
    }

    // Check if seed was requested before sendKey
    auto& state = states_[seed_level];
    if (!state.seed_requested) {
        return DiagErrorCode::SECURITY_ACCESS_DENIED;
    }

    // Verify key with SEC (pass the actual sendKey level)
    if (!sec_->verify_key(level, key)) {
        // Increment attempt count
        state.attempt_count++;
        state.seed_requested = false;

        // Check if max attempts exceeded
        if (state.attempt_count >= SecurityConfig::MAX_ATTEMPTS) {
            state.locked_until = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(SecurityConfig::LOCKOUT_DURATION_MS);
        }

        // 记录安全访问失败
        tbox::diag::DiagLogAdapter::session().warn(
            tbox::diag::events::SECURITY_ACCESS_FAILED,
            "Security access verification failed",
            {
                tbox::fw::log::Field(tbox::diag::events::fields::SECURITY_LEVEL, 
                    tbox::fw::log::FieldValue::makeString("0x" + std::to_string(level))),
                tbox::fw::log::Field(tbox::diag::events::fields::FAILURE_REASON, 
                    tbox::fw::log::FieldValue::makeString("invalid_key")),
                tbox::fw::log::Field(tbox::diag::events::fields::ATTEMPT_COUNT, 
                    tbox::fw::log::FieldValue::makeInt(state.attempt_count))
            }
        );

        return DiagErrorCode::SECURITY_ACCESS_DENIED;
    }

    // Success - unlock
    state.unlocked = true;
    state.seed_requested = false;
    state.attempt_count = 0;

    // 记录安全访问成功
    tbox::diag::DiagLogAdapter::session().info(
        tbox::diag::events::SECURITY_ACCESS_SUCCEEDED,
        "Security access verification succeeded",
        {
            tbox::fw::log::Field(tbox::diag::events::fields::SECURITY_LEVEL, 
                tbox::fw::log::FieldValue::makeString("0x" + std::to_string(level)))
        }
    );

    return DiagErrorCode::SUCCESS;
}

bool SecurityAccess::is_unlocked(uint8_t level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Normalize: if level is even (sendKey), use level-1 (requestSeed) for state lookup
    uint8_t seed_level = ((level & 0x01) == 0 && level > 0) ? (level - 1) : level;
    char level_buf[16], seed_buf[16];
    snprintf(level_buf, sizeof(level_buf), "0x%02X", level);
    snprintf(seed_buf, sizeof(seed_buf), "0x%02X", seed_level);
    DiagLogAdapter::session().debug(
        events::SECURITY_ACCESS_FAILED,
        "is_unlocked check",
        {fw::log::Field(events::fields::SECURITY_LEVEL,
            fw::log::FieldValue::makeString(level_buf)),
         fw::log::Field("seed_level",
            fw::log::FieldValue::makeString(seed_buf)),
         fw::log::Field("states_size",
            fw::log::FieldValue::makeInt(static_cast<int64_t>(states_.size())))}
    );

    // Check local state first
    auto it = states_.find(seed_level);
    if (it != states_.end()) {
        DiagLogAdapter::session().debug(
            events::SECURITY_ACCESS_FAILED,
            "is_unlocked found state",
            {fw::log::Field("unlocked",
                fw::log::FieldValue::makeBool(it->second.unlocked)),
             fw::log::Field("seed_requested",
                fw::log::FieldValue::makeBool(it->second.seed_requested))}
        );
        if (it->second.unlocked) {
            return true;
        }
    }

    // If local state doesn't show unlocked, check if SEC service has a valid seed
    // This handles the case where DIAG CLI is restarted but SEC service maintains state
    // For now, return false to require proper security access flow
    DiagLogAdapter::session().debug(
        events::SECURITY_ACCESS_FAILED,
        "is_unlocked state not found or not unlocked",
        {fw::log::Field("seed_level",
            fw::log::FieldValue::makeString(seed_buf))}
    );
    return false;
}

void SecurityAccess::lock(uint8_t level) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = states_.find(level);
    if (it != states_.end()) {
        it->second.unlocked = false;
        it->second.seed_requested = false;
    }
}

void SecurityAccess::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    states_.clear();
}

uint32_t SecurityAccess::get_attempt_count(uint8_t level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = states_.find(level);
    if (it != states_.end()) {
        return it->second.attempt_count;
    }
    return 0;
}

bool SecurityAccess::is_locked_out(uint8_t level) const {
    auto it = states_.find(level);
    if (it == states_.end()) {
        return false;
    }

    if (it->second.attempt_count < SecurityConfig::MAX_ATTEMPTS) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    return now < it->second.locked_until;
}

} // namespace diag
} // namespace tbox
