#include "real_prov.h"
#include "nrc_mapper.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include <iostream>
#include <chrono>

namespace tbox {
namespace diag {

RealProvAdapter::RealProvAdapter(std::shared_ptr<prov::ProvClient> client)
    : client_(std::move(client)) {}

DiagErrorCode RealProvAdapter::write_vin(const std::string& vin, const std::vector<uint8_t>& payload) {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "PROV IPC not connected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("prov")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("write_vin"))}
        );
        return DiagErrorCode::PROV_IPC_DISCONNECTED;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.write_vin",
        "PROV write_vin",
        {fw::log::Field("vin",
            fw::log::FieldValue::makeString(vin))}
    );

    auto result = client_->write_vin(vin);
    if (result != prov::ErrorCode::SUCCESS) {
        // 记录下游调用失败
        tbox::diag::DiagLogAdapter::downstream().error(
            tbox::diag::events::DOWNSTREAM_CALL_FAILED,
            "PROV IPC call failed",
            {
                tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM, 
                    tbox::fw::log::FieldValue::makeString("prov")),
                tbox::fw::log::Field(tbox::diag::events::fields::OPERATION, 
                    tbox::fw::log::FieldValue::makeString("write_vin")),
                tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM_ERROR_CODE, 
                    tbox::fw::log::FieldValue::makeInt(static_cast<int>(result)))
            }
        );
        return NrcMapper::prov_error_to_diag(static_cast<uint32_t>(result));
    }

    if (!payload.empty()) {
        DiagLogAdapter::downstream().info(
            "diag.downstream.write_vehicle_config",
            "PROV write_vehicle_config",
            {fw::log::Field("payload_size",
                fw::log::FieldValue::makeInt(static_cast<int64_t>(payload.size())))}
        );
        auto config_result = client_->write_vehicle_config(payload);
        if (config_result != prov::ErrorCode::SUCCESS) {
            DiagLogAdapter::downstream().error(
                events::DOWNSTREAM_CALL_FAILED,
                "PROV write_vehicle_config failed",
                {fw::log::Field(events::fields::DOWNSTREAM,
                    fw::log::FieldValue::makeString("prov")),
                 fw::log::Field(events::fields::OPERATION,
                    fw::log::FieldValue::makeString("write_vehicle_config")),
                 fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                    fw::log::FieldValue::makeString(
                        prov::error_code_to_string(config_result)))}
            );
            return NrcMapper::prov_error_to_diag(static_cast<uint32_t>(config_result));
        }
    }

    return DiagErrorCode::SUCCESS;
}

VinReadResult RealProvAdapter::read_vin() {
    VinReadResult result;

    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "PROV IPC not connected for read",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("prov")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("read_vin"))}
        );
        result.valid = false;
        return result;
    }

    DiagLogAdapter::downstream().debug(
        "diag.downstream.read_vin",
        "Calling PROV IPC read_vin"
    );
    auto t0 = std::chrono::steady_clock::now();
    result.vin = client_->read_vin();
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    DiagLogAdapter::downstream().info(
        "diag.downstream.read_vin_result",
        "PROV IPC read_vin returned",
        {fw::log::Field(events::fields::DURATION_MS,
            fw::log::FieldValue::makeInt(ms))}
    );

    auto t2 = std::chrono::steady_clock::now();
    auto state = client_->get_provision_state();
    auto t3 = std::chrono::steady_clock::now();
    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    DiagLogAdapter::downstream().debug(
        "diag.downstream.get_provision_state",
        "PROV get_provision_state returned",
        {fw::log::Field(events::fields::DURATION_MS,
            fw::log::FieldValue::makeInt(ms2))}
    );

    switch (state) {
        case prov::ProvisionState::NONE:
            result.bind_state = "NONE";
            break;
        case prov::ProvisionState::VIN_WRITTEN:
            result.bind_state = "VIN_WRITTEN";
            break;
        case prov::ProvisionState::BOUND:
            result.bind_state = "BOUND";
            break;
        case prov::ProvisionState::FAILED:
            result.bind_state = "FAILED";
            break;
        default:
            result.bind_state = "UNKNOWN";
            break;
    }

    result.valid = true;
    DiagLogAdapter::downstream().info(
        "diag.downstream.read_vin_complete",
        "PROV read_vin complete",
        {fw::log::Field("bind_state",
            fw::log::FieldValue::makeString(result.bind_state))}
    );

    return result;
}

bool RealProvAdapter::is_available() const {
    return client_ && client_->is_connected();
}

bool RealProvAdapter::reconnect() {
    if (!client_) {
        return false;
    }
    client_->disconnect();
    return client_->connect();
}

} // namespace diag
} // namespace tbox
