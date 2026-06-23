#include "security_access.h"
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

    // Check if seed was requested first
    auto it = states_.find(seed_level);
    if (it == states_.end() || !it->second.seed_requested) {
        return DiagErrorCode::SECURITY_ACCESS_DENIED;
    }

    // Check if already locked out
    if (is_locked_out(seed_level)) {
        return DiagErrorCode::SECURITY_ACCESS_DENIED;
    }

    // Verify key with SEC (pass the actual sendKey level)
    if (!sec_->verify_key(level, key)) {
        // Increment attempt count
        it->second.attempt_count++;
        it->second.seed_requested = false;

        // Check if max attempts exceeded
        if (it->second.attempt_count >= SecurityConfig::MAX_ATTEMPTS) {
            it->second.locked_until = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(SecurityConfig::LOCKOUT_DURATION_MS);
        }

        return DiagErrorCode::SECURITY_ACCESS_DENIED;
    }

    // Success - unlock
    it->second.unlocked = true;
    it->second.seed_requested = false;
    it->second.attempt_count = 0;

    return DiagErrorCode::SUCCESS;
}

bool SecurityAccess::is_unlocked(uint8_t level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Normalize: if level is even (sendKey), use level-1 (requestSeed) for state lookup
    uint8_t seed_level = ((level & 0x01) == 0 && level > 0) ? (level - 1) : level;
    auto it = states_.find(seed_level);
    if (it != states_.end()) {
        return it->second.unlocked;
    }
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
