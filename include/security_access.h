#pragma once

#include "data_models.h"
#include "constants.h"
#include "error_codes.h"
#include "sec_interface.h"
#include <memory>
#include <mutex>
#include <vector>
#include <map>

namespace tbox {
namespace diag {

class SecurityAccess {
public:
    explicit SecurityAccess(std::shared_ptr<SecInterface> sec);
    virtual ~SecurityAccess() = default;

    virtual DiagErrorCode request_seed(uint8_t level, std::vector<uint8_t>& seed);
    virtual DiagErrorCode send_key(uint8_t level, const std::vector<uint8_t>& key);
    virtual bool is_unlocked(uint8_t level) const;
    virtual void lock(uint8_t level);
    virtual void reset();
    virtual uint32_t get_attempt_count(uint8_t level) const;
    virtual bool is_locked_out(uint8_t level) const;

protected:
    std::shared_ptr<SecInterface> sec_;
    std::map<uint8_t, SecurityAccessState> states_;
    mutable std::mutex mutex_;
};

} // namespace diag
} // namespace tbox
