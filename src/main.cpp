#include <iostream>
#include <memory>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include "diag_service.h"
#include "doip_adapter.h"
#include "error_codes.h"
#include "stub_sec.h"
#include "stub_prov.h"

using namespace tbox::diag;

static std::atomic<bool> g_running{true};

static void signal_handler(int sig) {
    std::cout << "\nReceived signal " << sig << ", shutting down..." << std::endl;
    g_running = false;
}

int main() {
    std::cout << "TBOX DIAG Service Starting..." << std::endl;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    DoIpConfig doip_config;
    doip_config.listen_address = "0.0.0.0";
    doip_config.port = 13400;

    auto doip = std::make_shared<DoIpAdapter>(doip_config);
    if (!doip->start_server()) {
        std::cerr << "Failed to start DoIP server" << std::endl;
        return 1;
    }

    DiagServiceConfig config;
    config.config_file_path = "/etc/tbox/diag_config.yaml";

    auto service = std::make_unique<DiagService>(config);
    service->set_sec(std::make_shared<StubSecInterface>());
    service->set_prov(std::make_shared<StubProvInterface>());
    service->set_transport(doip);

    auto result = service->initialize();
    if (result != DiagErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize DIAG service: "
                  << error_code_to_string(result) << std::endl;
        return 1;
    }

    std::cout << "TBOX DIAG Service initialized successfully" << std::endl;
    std::cout << "DIAG service running on DoIP port " << doip_config.port
              << " (Ctrl+C to stop)" << std::endl;

    while (g_running) {
        service->process_pending_requests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "DIAG service shutting down..." << std::endl;
    service->shutdown();
    std::cout << "DIAG service stopped." << std::endl;

    return 0;
}
