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
#include "prov_client.h"
#include "prov_to_sec_adapter.h"
#include "config.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include "diag_context.h"

using namespace tbox::diag;

static std::atomic<bool> g_running{true};

static void signal_handler(int sig) {
    std::cout << "\nReceived signal " << sig << ", shutting down..." << std::endl;
    g_running = false;
}

int main() {
    tbox::diag::DiagLogAdapter::transport().info(
        "diag.service.starting",
        "TBOX DIAG Service Starting"
    );

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Load DIAG configuration using framework-config
    auto& config_manager = hwyz::config::ConfigManager::instance();
    auto config_result = config_manager.load("diag");
    if (config_result != hwyz::config::ConfigError::kOk) {
        tbox::diag::DiagLogAdapter::transport().fatal(
            "diag.config.load_failed",
            "Failed to load DIAG configuration",
            {tbox::fw::log::Field("error_code",
                tbox::fw::log::FieldValue::makeInt(static_cast<uint32_t>(config_result)))}
        );
        return 1;
    }

    auto config_snapshot = config_manager.getSnapshot();
    if (!config_snapshot) {
        tbox::diag::DiagLogAdapter::transport().fatal(
            "diag.config.snapshot_failed",
            "Failed to get configuration snapshot"
        );
        return 1;
    }

    // 读取日志配置
    tbox::fw::log::LogConfig log_config;
    log_config.strict = true;  // fail-closed
    
    // 从 common.log.* 读取基础配置
    std::string log_level_str = config_snapshot->getString("common.log.level", "info");
    if (log_level_str == "trace") log_config.level = tbox::fw::log::LogLevel::kTrace;
    else if (log_level_str == "debug") log_config.level = tbox::fw::log::LogLevel::kDebug;
    else if (log_level_str == "info") log_config.level = tbox::fw::log::LogLevel::kInfo;
    else if (log_level_str == "warn") log_config.level = tbox::fw::log::LogLevel::kWarn;
    else if (log_level_str == "error") log_config.level = tbox::fw::log::LogLevel::kError;
    
    log_config.async_config.enabled = config_snapshot->getBool("common.log.async.enabled", true);
    log_config.async_config.queue_size = config_snapshot->getInt("common.log.async.queue_size", 4096);
    log_config.async_config.flush_interval_ms = config_snapshot->getInt("common.log.async.flush_interval_ms", 1000);
    
    // 从 diag.log.* 读取 DIAG 特定配置
    std::string diag_log_level = config_snapshot->getString("diag.log.level", "");
    if (!diag_log_level.empty()) {
        if (diag_log_level == "trace") log_config.module_levels["uds_router"] = tbox::fw::log::LogLevel::kTrace;
        else if (diag_log_level == "debug") log_config.module_levels["uds_router"] = tbox::fw::log::LogLevel::kDebug;
    }
    
    // 初始化 Logger
    auto log_result = tbox::diag::DiagLogAdapter::init("diag", log_config);
    if (log_result.error != tbox::fw::log::LogError::kOk) {
        // Logger 未初始化，只能 fallback 到 stderr
        std::cerr << "FATAL: Logger init failed: " << log_result.error_message << std::endl;
        return 1;
    }
    
    // 记录初始化成功
    tbox::diag::DiagLogAdapter::transport().info(
        tbox::diag::events::TRANSPORT_CONNECTED,
        "DIAG Logger initialized successfully"
    );

    // Read DoIP configuration from config
    DoIpConfig doip_config;
    doip_config.listen_address = config_snapshot->getString("transport.doip.listen_address", "0.0.0.0");
    doip_config.port = config_snapshot->getInt("transport.doip.port", 13400);

    auto doip = std::make_shared<DoIpAdapter>(doip_config);
    if (!doip->start_server()) {
        tbox::diag::DiagLogAdapter::transport().fatal(
            tbox::diag::events::TRANSPORT_LISTEN_FAILED,
            "Failed to start DoIP server"
        );
        return 1;
    }

    DiagServiceConfig config;
    config.config_snapshot = config_snapshot;

    auto service = std::make_unique<DiagService>(config);

    // 通过 IPC 连接到 PROV 服务
    std::string prov_socket_path = config_snapshot->getString("prov.ipc_socket_path", "/tmp/tbox-prov.sock");
    auto prov_client = std::make_shared<tbox::prov::ProvClient>(prov_socket_path);
    if (!prov_client->connect()) {
        tbox::diag::DiagLogAdapter::downstream().fatal(
            tbox::diag::events::PROV_CONNECTION_FAILED,
            "Failed to connect to PROV IPC service",
            {tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM,
                tbox::fw::log::FieldValue::makeString("prov")),
             tbox::fw::log::Field("socket_path",
                tbox::fw::log::FieldValue::makeString(prov_socket_path))}
        );
        return 1;
    }
    tbox::diag::DiagLogAdapter::downstream().info(
        tbox::diag::events::PROV_CONNECTED,
        "Connected to PROV IPC service",
        {tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM,
            tbox::fw::log::FieldValue::makeString("prov")),
         tbox::fw::log::Field("socket_path",
            tbox::fw::log::FieldValue::makeString(prov_socket_path))}
    );
    service->set_prov(std::make_shared<RealProvAdapter>(prov_client));

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
    sec_service->set_prov_service(std::make_shared<ProvToSecAdapter>(prov_client));
    auto sec_init = sec_service->initialize();
    if (sec_init != tbox::sec::ErrorCode::SUCCESS) {
        tbox::diag::DiagLogAdapter::downstream().fatal(
            tbox::diag::events::DOWNSTREAM_CALL_FAILED,
            "Failed to initialize SEC service",
            {tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM,
                tbox::fw::log::FieldValue::makeString("sec")),
             tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM_ERROR_CODE,
                tbox::fw::log::FieldValue::makeString(
                    tbox::sec::error_code_to_string(sec_init)))}
        );
        return 1;
    }
    service->set_sec(std::make_shared<RealSecAdapter>(sec_service));
    service->set_transport(doip);

    auto result = service->initialize();
    if (result != DiagErrorCode::SUCCESS) {
        tbox::diag::DiagLogAdapter::transport().fatal(
            "diag.service.init_failed",
            "Failed to initialize DIAG service",
            {tbox::fw::log::Field("error_code",
                tbox::fw::log::FieldValue::makeString(error_code_to_string(result)))}
        );
        return 1;
    }

    tbox::diag::DiagLogAdapter::transport().info(
        tbox::diag::events::SERVICE_INITIALIZED,
        "TBOX DIAG Service initialized successfully",
        {tbox::fw::log::Field("doip_port",
            tbox::fw::log::FieldValue::makeInt(doip_config.port))}
    );

    while (g_running) {
        service->process_pending_requests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    tbox::diag::DiagLogAdapter::transport().info(
        tbox::diag::events::SERVICE_SHUTTING_DOWN,
        "DIAG service shutting down"
    );
    service->shutdown();
    tbox::diag::DiagLogAdapter::transport().info(
        tbox::diag::events::SERVICE_STOPPED,
        "DIAG service stopped"
    );

    return 0;
}
