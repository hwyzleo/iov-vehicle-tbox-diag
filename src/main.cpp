#include <iostream>
#include <memory>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include "diag_service.h"
#include "doip_adapter.h"
#include "error_codes.h"
#include "real_sec_adapter.h"
#include "sec_service.h"
#include "real_prov.h"
#include "prov_service.h"
#include "prov_to_sec_adapter.h"

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

    // 初始化PROV服务（先于SEC，以便获取VIN和ECU UID）
    tbox::prov::ProvServiceConfig prov_config;
    prov_config.storage_path = "/var/tbox/prov";
    prov_config.enable_write_protection = true;
    auto prov_service = std::make_shared<tbox::prov::ProvService>(prov_config);
    auto prov_init = prov_service->initialize();
    if (prov_init != tbox::prov::ErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize PROV service: "
                  << tbox::prov::error_code_to_string(prov_init) << std::endl;
        return 1;
    }
    service->set_prov(std::make_shared<RealProvAdapter>(prov_service));

    // 创建SEC服务所需目录
    std::filesystem::create_directories("/var/tbox");

    // 创建空状态文件（如果不存在）
    std::string state_file = "/var/tbox/sec_state.json";
    if (!std::filesystem::exists(state_file)) {
        std::ofstream f(state_file);
        f << "{}";
        f.close();
    }

    // 初始化SEC服务
    tbox::sec::SecServiceConfig sec_config;
    sec_config.hsm_type = "software";
    sec_config.hsm_config_path = "/etc/tbox/hsm_config.yaml";
    sec_config.state_file_path = state_file;
    sec_config.ca_cert_path = "/Users/hwyz_leo/Docker/step/certs/intermediate_ca.crt";
    sec_config.cloud_config.oapi_endpoint = "https://oapi.example.com";
    sec_config.cloud_config.timeout_ms = 30000;
    sec_config.cloud_config.retry_count = 3;
    sec_config.cloud_config.retry_delay_ms = 1000;

    auto sec_service = std::make_shared<tbox::sec::SecService>(sec_config);
    sec_service->set_prov_service(std::make_shared<ProvToSecAdapter>(prov_service));
    auto sec_init = sec_service->initialize();
    if (sec_init != tbox::sec::ErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize SEC service: "
                  << tbox::sec::error_code_to_string(sec_init) << std::endl;
        return 1;
    }
    service->set_sec(std::make_shared<RealSecAdapter>(sec_service));
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
