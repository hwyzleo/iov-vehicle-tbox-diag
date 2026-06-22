#pragma once

#include "data_models.h"
#include "constants.h"
#include "error_codes.h"
#include "session_manager.h"
#include "security_access.h"
#include "service_dispatcher.h"
#include "nrc_mapper.h"
#include "transport_adapter.h"
#include "prov_interface.h"
#include "sec_interface.h"
#include <memory>
#include <vector>
#include <mutex>

namespace tbox {
namespace diag {

struct DiagServiceConfig {
    std::string config_file_path;
};

class DiagService {
public:
    DiagService();
    explicit DiagService(const DiagServiceConfig& config);
    virtual ~DiagService() = default;

    virtual DiagErrorCode initialize();
    virtual DiagResponse process_request(const DiagRequest& request);
    virtual void process_pending_requests();
    virtual void shutdown();

    virtual bool is_initialized() const;
    virtual DiagSession get_current_session() const;

    // For testing: inject dependencies
    virtual void set_transport(std::shared_ptr<TransportAdapter> transport);
    virtual void set_prov(std::shared_ptr<ProvInterface> prov);
    virtual void set_sec(std::shared_ptr<SecInterface> sec);

protected:
    DiagServiceConfig config_;
    bool initialized_ = false;

    std::shared_ptr<TransportAdapter> transport_;
    std::shared_ptr<ProvInterface> prov_;
    std::shared_ptr<SecInterface> sec_;

    std::shared_ptr<SessionManager> session_mgr_;
    std::shared_ptr<SecurityAccess> security_access_;
    std::shared_ptr<ServiceDispatcher> dispatcher_;

    mutable std::mutex mutex_;

    virtual DiagErrorCode initialize_submodules();
    virtual DiagErrorCode register_default_routes();
};

} // namespace diag
} // namespace tbox
