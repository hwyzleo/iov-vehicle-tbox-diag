#pragma once

#include "data_models.h"
#include "constants.h"
#include "error_codes.h"
#include "prov_interface.h"
#include "session_manager.h"
#include "security_access.h"
#include <memory>
#include <map>
#include <functional>
#include <vector>

namespace tbox {
namespace diag {

class ServiceDispatcher {
public:
    ServiceDispatcher(std::shared_ptr<ProvInterface> prov,
                      std::shared_ptr<SessionManager> session_mgr,
                      std::shared_ptr<SecurityAccess> security_access);
    virtual ~ServiceDispatcher() = default;

    virtual DiagResponse dispatch(const DiagRequest& request);

    virtual void register_route(uint8_t service_id, uint16_t did_or_rid,
                                const std::string& downstream, bool requires_unlock);

protected:
    DiagResponse handle_session_control(const DiagRequest& request);
    DiagResponse handle_tester_present(const DiagRequest& request);
    DiagResponse handle_security_access(const DiagRequest& request);
    DiagResponse handle_routine_control(const DiagRequest& request);
    DiagResponse handle_read_data_by_identifier(const DiagRequest& request);

    DiagResponse create_positive_response(uint8_t service_id, uint8_t sub_function,
                                          const std::vector<uint8_t>& data);
    DiagResponse create_negative_response(uint8_t service_id, uint8_t nrc,
                                          const std::string& diag_error_code);

    bool check_security_required(uint8_t service_id, uint16_t did_or_rid);

    std::shared_ptr<ProvInterface> prov_;
    std::shared_ptr<SessionManager> session_mgr_;
    std::shared_ptr<SecurityAccess> security_access_;
    std::vector<RoutingEntry> routing_table_;
};

} // namespace diag
} // namespace tbox
