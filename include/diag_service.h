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
#include "config.h"
#include "ipc_types.h"
#include <memory>
#include <vector>
#include <mutex>

namespace tbox {
namespace fw { namespace ipc { class Server; } }
}

namespace tbox {
namespace diag {

class DiagIpcDispatcher;

struct DiagServiceConfig {
    std::string config_file_path;
    std::shared_ptr<const hwyz::config::ImmutableConfigView> config_snapshot;

    // IPC configuration (framework-ipc)
    ::tbox::fw::ipc::IpcConfig ipc_config{};
    std::string ipc_socket_path = "/tmp/tbox-diag.sock";
};

class DiagService {
public:
    DiagService();
    explicit DiagService(const DiagServiceConfig& config);
    virtual ~DiagService();

    virtual DiagErrorCode initialize();
    virtual DiagResponse process_request(const DiagRequest& request);
    virtual void process_pending_requests();
    virtual void shutdown();

    virtual bool is_initialized() const;
    virtual DiagSession get_current_session() const;

    // IPC server (framework-ipc)
    virtual bool start_ipc_server();
    virtual void stop_ipc_server();

    // IPC query helpers (used by DiagIpcDispatcher)
    virtual bool is_tester_connected() const;
    virtual VinReadResult get_vehicle_info();

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

#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
    std::unique_ptr<::tbox::fw::ipc::Server> fw_ipc_server_;
    std::unique_ptr<DiagIpcDispatcher> ipc_dispatcher_;
#endif

    mutable std::mutex mutex_;

    virtual DiagErrorCode initialize_submodules();
    virtual DiagErrorCode register_default_routes();
    virtual DiagErrorCode load_config();
    virtual DiagErrorCode apply_config();
};

} // namespace diag
} // namespace tbox
