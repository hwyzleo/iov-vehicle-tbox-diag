#include "diag_service.h"
#include "real_prov.h"
#include "prov_client.h"
#include "sec_ipc_adapter.h"
#include "sec_client.h"
#include "config.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace tbox::diag;

void print_usage() {
    std::cout << "DIAG CLI Tool" << std::endl;
    std::cout << "Usage: diag_cli [--config <path>] <command> [args...]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --config <path>  - Config root path (default: /etc/tbox/)" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  init                          - Initialize DIAG service" << std::endl;
    std::cout << "  session <type>                - Switch session (default/extended/programming)" << std::endl;
    std::cout << "  tester_present                - Send TesterPresent" << std::endl;
    std::cout << "  security_access <level>       - Request security access (seed)" << std::endl;
    std::cout << "  send_key <level> <hex_key>    - Send security key" << std::endl;
    std::cout << "  read_vin                      - Read VIN (DID 0xF190)" << std::endl;
    std::cout << "  read_binding                  - Read binding state (DID 0xF191)" << std::endl;
    std::cout << "  write_vin <vin>               - Write VIN via routine (RID 0xFF00)" << std::endl;
    std::cout << "  generate_key_pair             - Generate key pair (RID 0xFF01)" << std::endl;
    std::cout << "  read_csr                      - Read CSR (RID 0xFF02)" << std::endl;
    std::cout << "  inject_cert <hex_cert>        - Inject certificate (RID 0xFF03)" << std::endl;
    std::cout << "  raw <hex_request>             - Send raw UDS request" << std::endl;
    std::cout << "  status                        - Show current session status" << std::endl;
}

std::string bytes_to_hex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) 
            << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 < hex.length()) {
            uint8_t byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
            bytes.push_back(byte);
        }
    }
    return bytes;
}

void print_response(const DiagResponse& response) {
    if (response.positive) {
        std::cout << "Positive Response:" << std::endl;
        std::cout << "  Service ID: 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(2) << static_cast<int>(response.service_id) << std::endl;
        if (response.sub_function != 0) {
            std::cout << "  Sub Function: 0x" << std::hex << std::uppercase << std::setfill('0') 
                      << std::setw(2) << static_cast<int>(response.sub_function) << std::endl;
        }
        if (!response.payload.empty()) {
            std::cout << "  Payload: " << bytes_to_hex(response.payload) << std::endl;
        }
    } else {
        std::cout << "Negative Response:" << std::endl;
        std::cout << "  NRC: 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(2) << static_cast<int>(response.nrc) << std::endl;
        if (!response.diag_error_code.empty()) {
            std::cout << "  Error Code: " << response.diag_error_code << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string config_root = "/etc/tbox/";
    int command_index = 1;

    // 解析参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_root = argv[++i];
        } else {
            command_index = i;
            break;
        }
    }

    if (command_index >= argc) {
        print_usage();
        return 1;
    }

    std::string command = argv[command_index];

    // 加载框架配置
    auto err = CONFIG_MANAGER.load("diag", config_root);
    if (err != hwyz::config::ConfigError::kOk) {
        auto info = CONFIG_MANAGER.getLastError();
        std::cerr << "Warning: Config load failed (" << info.message 
                  << "), using default config" << std::endl;
    }

    // 创建 DIAG 服务配置
    DiagServiceConfig config;
    config.config_file_path = "diag.yaml";
    config.config_snapshot = CONFIG_SNAPSHOT;

    // 创建 DIAG 服务实例
    DiagService service(config);

    // 通过 IPC 连接到 PROV 服务
    std::string prov_socket_path = "/tmp/tbox-prov.sock";
    if (CONFIG_SNAPSHOT) {
        prov_socket_path = CONFIG_SNAPSHOT->getString("prov.ipc_socket_path", prov_socket_path);
    }
    auto prov_client = std::make_shared<tbox::prov::ProvClient>(prov_socket_path);
    if (!prov_client->connect()) {
        std::cerr << "Failed to connect to PROV IPC service at " << prov_socket_path << std::endl;
        return 1;
    }
    std::cout << "Connected to PROV IPC service at " << prov_socket_path << std::endl;
    service.set_prov(std::make_shared<RealProvAdapter>(prov_client));

    // 通过 IPC 连接到 SEC 服务
    std::string sec_socket_path = "/tmp/tbox-sec.sock";
    if (CONFIG_SNAPSHOT) {
        sec_socket_path = CONFIG_SNAPSHOT->getString("sec.ipc_socket_path", sec_socket_path);
    }
    auto sec_client = std::make_shared<tbox::sec::SecClient>(sec_socket_path);
    if (!sec_client->connect()) {
        std::cerr << "Failed to connect to SEC IPC service at " << sec_socket_path << std::endl;
        return 1;
    }
    std::cout << "Connected to SEC IPC service at " << sec_socket_path << std::endl;
    service.set_sec(std::make_shared<SecIpcAdapter>(sec_client));

    // 初始化服务
    auto result = service.initialize();
    if (result != DiagErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize DIAG service: " 
                  << static_cast<int>(result) << std::endl;
        return 1;
    }

    if (command == "init") {
        std::cout << "DIAG service initialized successfully" << std::endl;
    }
    else if (command == "session") {
        if (command_index + 1 >= argc) {
            std::cerr << "Usage: diag_cli session <type>" << std::endl;
            std::cerr << "Types: default, extended, programming" << std::endl;
            return 1;
        }
        std::string session_type = argv[command_index + 1];
        uint8_t session_id = 0;
        if (session_type == "default") {
            session_id = UdsSession::DEFAULT;
        } else if (session_type == "extended") {
            session_id = UdsSession::EXTENDED;
        } else if (session_type == "programming") {
            session_id = UdsSession::PROGRAMMING;
        } else {
            std::cerr << "Invalid session type: " << session_type << std::endl;
            return 1;
        }

        DiagRequest request;
        request.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
        request.sub_function = session_id;
        request.source_address = 0x0E80;  // Default tester address
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "tester_present") {
        DiagRequest request;
        request.service_id = UdsService::TESTER_PRESENT;
        request.sub_function = 0x00;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "security_access") {
        if (command_index + 1 >= argc) {
            std::cerr << "Usage: diag_cli security_access <level>" << std::endl;
            return 1;
        }
        uint8_t level = static_cast<uint8_t>(std::stoi(argv[command_index + 1], nullptr, 16));

        DiagRequest request;
        request.service_id = UdsService::SECURITY_ACCESS;
        request.sub_function = level;  // Request seed
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "send_key") {
        if (command_index + 2 >= argc) {
            std::cerr << "Usage: diag_cli send_key <level> <hex_key>" << std::endl;
            return 1;
        }
        uint8_t level = static_cast<uint8_t>(std::stoi(argv[command_index + 1], nullptr, 16));
        std::string hex_key = argv[command_index + 2];
        auto key_bytes = hex_to_bytes(hex_key);

        DiagRequest request;
        request.service_id = UdsService::SECURITY_ACCESS;
        request.sub_function = level | 0x80;  // Send key (set MSB)
        request.payload = key_bytes;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "read_vin") {
        DiagRequest request;
        request.service_id = UdsService::READ_DATA_BY_IDENTIFIER;
        request.did_or_rid = Did::VIN;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
        // Skip DID echo (2 bytes) to get VIN
        if (response.positive && response.payload.size() > 2) {
            std::string vin(response.payload.begin() + 2, response.payload.end());
            std::cout << "  VIN: " << vin << std::endl;
        }
    }
    else if (command == "read_binding") {
        DiagRequest request;
        request.service_id = UdsService::READ_DATA_BY_IDENTIFIER;
        request.did_or_rid = Did::BINDING_STATE;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "write_vin") {
        if (command_index + 1 >= argc) {
            std::cerr << "Usage: diag_cli write_vin <vin>" << std::endl;
            return 1;
        }
        std::string vin = argv[command_index + 1];
        std::vector<uint8_t> vin_data(vin.begin(), vin.end());

        // Step 1: Switch to extended session
        DiagRequest session_req;
        session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
        session_req.sub_function = UdsSession::EXTENDED;
        session_req.source_address = 0x0E80;
        session_req.transport = TransportType::DOIP;
        auto session_resp = service.process_request(session_req);
        if (!session_resp.positive) {
            std::cerr << "Failed to switch session" << std::endl;
            print_response(session_resp);
            return 1;
        }

        // Step 2: Request security access seed
        DiagRequest seed_req;
        seed_req.service_id = UdsService::SECURITY_ACCESS;
        seed_req.sub_function = 0x27;  // Request seed level 0x27
        seed_req.source_address = 0x0E80;
        seed_req.transport = TransportType::DOIP;
        auto seed_resp = service.process_request(seed_req);
        if (!seed_resp.positive) {
            std::cerr << "Failed to request seed" << std::endl;
            print_response(seed_resp);
            return 1;
        }

        // Step 3: Compute key (seed XOR 0x01)
        std::vector<uint8_t> key;
        for (size_t i = 0; i < seed_resp.payload.size(); i++) {
            key.push_back(seed_resp.payload[i] ^ 0x01);
        }

        // Step 4: Send key
        DiagRequest key_req;
        key_req.service_id = UdsService::SECURITY_ACCESS;
        key_req.sub_function = 0x28;  // Send key (requestSeed level + 1)
        key_req.payload = key;
        key_req.source_address = 0x0E80;
        key_req.transport = TransportType::DOIP;
        auto key_resp = service.process_request(key_req);
        if (!key_resp.positive) {
            std::cerr << "Failed to verify key" << std::endl;
            print_response(key_resp);
            return 1;
        }

        // Step 5: Write VIN
        DiagRequest request;
        request.service_id = UdsService::ROUTINE_CONTROL;
        request.sub_function = 0x01;  // Start routine
        request.did_or_rid = Rid::WRITE_VIN_ROUTINE;
        request.payload = vin_data;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "generate_key_pair") {
        DiagRequest request;
        request.service_id = UdsService::ROUTINE_CONTROL;
        request.sub_function = 0x01;  // Start routine
        request.did_or_rid = Rid::GENERATE_KEY_PAIR;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "read_csr") {
        DiagRequest request;
        request.service_id = UdsService::ROUTINE_CONTROL;
        request.sub_function = 0x01;  // Start routine
        request.did_or_rid = Rid::READ_CSR;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
        if (response.positive && !response.payload.empty()) {
            std::cout << "  CSR (hex): " << bytes_to_hex(response.payload) << std::endl;
        }
    }
    else if (command == "inject_cert") {
        if (command_index + 1 >= argc) {
            std::cerr << "Usage: diag_cli inject_cert <hex_cert>" << std::endl;
            return 1;
        }
        std::string hex_cert = argv[command_index + 1];
        auto cert_bytes = hex_to_bytes(hex_cert);

        DiagRequest request;
        request.service_id = UdsService::ROUTINE_CONTROL;
        request.sub_function = 0x01;  // Start routine
        request.did_or_rid = Rid::INJECT_CERTIFICATE;
        request.payload = cert_bytes;
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "raw") {
        if (command_index + 1 >= argc) {
            std::cerr << "Usage: diag_cli raw <hex_request>" << std::endl;
            return 1;
        }
        std::string hex_request = argv[command_index + 1];
        auto request_bytes = hex_to_bytes(hex_request);

        if (request_bytes.empty()) {
            std::cerr << "Invalid hex request" << std::endl;
            return 1;
        }

        DiagRequest request;
        request.service_id = request_bytes[0];
        if (request_bytes.size() > 1) {
            request.sub_function = request_bytes[1];
        }
        if (request_bytes.size() > 3) {
            request.did_or_rid = (static_cast<uint16_t>(request_bytes[2]) << 8) | request_bytes[3];
        }
        if (request_bytes.size() > 4) {
            request.payload.assign(request_bytes.begin() + 4, request_bytes.end());
        }
        request.source_address = 0x0E80;
        request.transport = TransportType::DOIP;

        auto response = service.process_request(request);
        print_response(response);
    }
    else if (command == "status") {
        auto session = service.get_current_session();
        std::cout << "Current Session Status:" << std::endl;
        std::cout << "  Source Address: 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(4) << session.source_address << std::endl;
        std::cout << "  Session Type: ";
        switch (session.session_type) {
            case SessionType::DEFAULT: std::cout << "DEFAULT"; break;
            case SessionType::PROGRAMMING: std::cout << "PROGRAMMING"; break;
            case SessionType::EXTENDED: std::cout << "EXTENDED"; break;
        }
        std::cout << std::endl;
        std::cout << "  Session State: ";
        switch (session.state) {
            case SessionState::IDLE: std::cout << "IDLE"; break;
            case SessionState::ACTIVE: std::cout << "ACTIVE"; break;
            case SessionState::SECURITY_UNLOCKED: std::cout << "SECURITY_UNLOCKED"; break;
            case SessionState::TIMED_OUT: std::cout << "TIMED_OUT"; break;
        }
        std::cout << std::endl;
        std::cout << "  Security Unlocked: " << (session.security_unlocked ? "true" : "false") << std::endl;
        std::cout << "  Transport: " << (session.transport == TransportType::DOIP ? "DoIP" : "DoCAN") << std::endl;
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        print_usage();
        return 1;
    }

    return 0;
}
