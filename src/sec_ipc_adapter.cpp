#include "sec_ipc_adapter.h"
#include <iostream>

namespace tbox {
namespace diag {

SecIpcAdapter::SecIpcAdapter(std::shared_ptr<sec::SecClient> client)
    : client_(std::move(client)) {}

bool SecIpcAdapter::get_seed(uint8_t level, std::vector<uint8_t>& seed) {
    if (!is_available()) {
        std::cerr << "[SEC-IPC] Client not connected" << std::endl;
        return false;
    }

    auto result = client_->get_seed(level, seed);
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[SEC-IPC] get_seed failed: " << static_cast<int>(result) << std::endl;
        return false;
    }

    std::cout << "[SEC-IPC] get_seed level=0x" << std::hex << (int)level 
              << " seed_size=" << std::dec << seed.size() << std::endl;
    return true;
}

bool SecIpcAdapter::verify_key(uint8_t level, const std::vector<uint8_t>& key) {
    if (!is_available()) {
        std::cerr << "[SEC-IPC] Client not connected" << std::endl;
        return false;
    }

    auto result = client_->verify_key(level, key);
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[SEC-IPC] verify_key failed: " << static_cast<int>(result) << std::endl;
        return false;
    }

    std::cout << "[SEC-IPC] verify_key level=0x" << std::hex << (int)level << " SUCCESS" << std::endl;
    return true;
}

bool SecIpcAdapter::is_available() const {
    return client_ && client_->is_connected();
}

bool SecIpcAdapter::generate_key_pair() {
    if (!is_available()) {
        std::cerr << "[SEC-IPC] Client not connected" << std::endl;
        return false;
    }

    auto result = client_->generate_key_pair();
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[SEC-IPC] generate_key_pair failed: " << static_cast<int>(result) << std::endl;
        return false;
    }

    std::cout << "[SEC-IPC] generate_key_pair SUCCESS" << std::endl;
    return true;
}

bool SecIpcAdapter::get_csr(std::vector<uint8_t>& csr_der) {
    if (!is_available()) {
        std::cerr << "[SEC-IPC] Client not connected" << std::endl;
        return false;
    }

    auto result = client_->get_csr(csr_der);
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[SEC-IPC] get_csr failed: " << static_cast<int>(result) << std::endl;
        return false;
    }

    std::cout << "[SEC-IPC] get_csr size=" << csr_der.size() << std::endl;
    return true;
}

bool SecIpcAdapter::submit_csr() {
    if (!is_available()) {
        std::cerr << "[SEC-IPC] Client not connected" << std::endl;
        return false;
    }

    auto result = client_->submit_csr();
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[SEC-IPC] submit_csr failed: " << static_cast<int>(result) << std::endl;
        return false;
    }

    std::cout << "[SEC-IPC] submit_csr SUCCESS" << std::endl;
    return true;
}

bool SecIpcAdapter::inject_certificate(const std::vector<uint8_t>& cert_der) {
    if (!is_available()) {
        std::cerr << "[SEC-IPC] Client not connected" << std::endl;
        return false;
    }

    auto result = client_->inject_certificate(cert_der);
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[SEC-IPC] inject_certificate failed: " << static_cast<int>(result) << std::endl;
        return false;
    }

    std::cout << "[SEC-IPC] inject_certificate SUCCESS" << std::endl;
    return true;
}

} // namespace diag
} // namespace tbox