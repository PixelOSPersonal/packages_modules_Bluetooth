//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "service/ipc/binder/bluetooth_gatt_server_binder_server.h"

#include <base/logging.h>

#include "service/adapter.h"

using ::android::String8;
using ::android::String16;
using ::android::binder::Status;

using ::android::bluetooth::IBluetoothGattServerCallback;

namespace ipc {
namespace binder {

namespace {
const int kInvalidInstanceId = -1;
}  // namespace

BluetoothGattServerBinderServer::BluetoothGattServerBinderServer(
    bluetooth::Adapter* adapter)
    : adapter_(adapter) {
  CHECK(adapter_);
}

Status BluetoothGattServerBinderServer::RegisterServer(
    const ::android::sp<IBluetoothGattServerCallback>& callback,
    bool* _aidl_return) {
  VLOG(2) << __func__;
  bluetooth::GattServerFactory* gatt_server_factory =
      adapter_->GetGattServerFactory();

  *_aidl_return = RegisterInstanceBase(callback, gatt_server_factory);
  return Status::ok();
}

Status BluetoothGattServerBinderServer::UnregisterServer(int server_id) {
  VLOG(2) << __func__;
  UnregisterInstanceBase(server_id);
  return Status::ok();
}

Status BluetoothGattServerBinderServer::UnregisterAll() {
  VLOG(2) << __func__;
  UnregisterAllBase();
  return Status::ok();
}

Status BluetoothGattServerBinderServer::BeginServiceDeclaration(
    int server_id, bool is_primary, const android::bluetooth::UUID& uuid,
    android::bluetooth::GattIdentifier* out_id, bool* _aidl_return) {
  VLOG(2) << __func__;
  CHECK(out_id);
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    *_aidl_return = false;
    return Status::ok();
  }

  auto service_id = gatt_server->BeginServiceDeclaration(uuid, is_primary);
  if (!service_id) {
    LOG(ERROR) << "Failed to begin service declaration - server_id: "
               << server_id << " UUID: " << uuid.ToString();
    *_aidl_return = false;
    return Status::ok();
  }

  *out_id = *service_id;

  *_aidl_return = true;
  return Status::ok();
}

Status BluetoothGattServerBinderServer::AddCharacteristic(
    int server_id, const android::bluetooth::UUID& uuid, int properties,
    int permissions, android::bluetooth::GattIdentifier* out_id,
    bool* _aidl_return) {
  VLOG(2) << __func__;
  CHECK(out_id);
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    *_aidl_return = false;
    return Status::ok();
  }

  auto char_id = gatt_server->AddCharacteristic(uuid, properties, permissions);
  if (!char_id) {
    LOG(ERROR) << "Failed to add characteristic - server_id: " << server_id
               << " UUID: " << uuid.ToString();
    *_aidl_return = false;
    return Status::ok();
  }

  *out_id = *char_id.get();
  *_aidl_return = true;
  return Status::ok();
}

Status BluetoothGattServerBinderServer::AddDescriptor(
    int server_id, const android::bluetooth::UUID& uuid, int permissions,
    android::bluetooth::GattIdentifier* out_id, bool* _aidl_return) {
  VLOG(2) << __func__;
  CHECK(out_id);
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    *_aidl_return = false;
    return Status::ok();
  }

  auto desc_id = gatt_server->AddDescriptor(uuid, permissions);
  if (!desc_id) {
    LOG(ERROR) << "Failed to add descriptor - server_id: " << server_id
               << " UUID: " << uuid.ToString();
    *_aidl_return = false;
    return Status::ok();
  }

  *out_id = *desc_id.get();
  *_aidl_return = true;
  return Status::ok();
}

Status BluetoothGattServerBinderServer::EndServiceDeclaration(
    int server_id, bool* _aidl_return) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    *_aidl_return = false;
    return Status::ok();
  }

  // Create a weak pointer and pass that to the callback to prevent a potential
  // use after free.
  android::wp<BluetoothGattServerBinderServer> weak_ptr_to_this(this);
  auto callback = [=](bluetooth::BLEStatus status,
                      const bluetooth::GattIdentifier& service_id) {
    auto sp_to_this = weak_ptr_to_this.promote();
    if (!sp_to_this.get()) {
      VLOG(2) << "BluetoothLowEnergyBinderServer was deleted";
      return;
    }

    std::lock_guard<std::mutex> lock(*maps_lock());

    auto gatt_cb = GetGattServerCallback(server_id);
    if (!gatt_cb.get()) {
      VLOG(2) << "The callback was deleted";
      return;
    }

    gatt_cb->OnServiceAdded(status, service_id);
  };

  if (!gatt_server->EndServiceDeclaration(callback)) {
    LOG(ERROR) << "Failed to end service declaration";
    *_aidl_return = false;
    return Status::ok();
  }

  *_aidl_return = true;
  return Status::ok();
}

Status BluetoothGattServerBinderServer::SendResponse(
    int server_id, const String16& device_address, int request_id, int status,
    int offset, const std::vector<uint8_t>& value, bool* _aidl_return) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    *_aidl_return = false;
    return Status::ok();
  }

  *_aidl_return = gatt_server->SendResponse(
      std::string(String8(device_address).string()), request_id,
      static_cast<bluetooth::GATTError>(status), offset, value);

  return Status::ok();
}

Status BluetoothGattServerBinderServer::SendNotification(
    int server_id, const String16& device_address,
    const android::bluetooth::GattIdentifier& characteristic_id, bool confirm,
    const std::vector<uint8_t>& value, bool* _aidl_return) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_server = GetGattServer(server_id);
  if (!gatt_server) {
    LOG(ERROR) << "Unknown server_id: " << server_id;
    *_aidl_return = false;
    return Status::ok();
  }

  // Create a weak pointer and pass that to the callback to prevent a potential
  // use after free.
  android::wp<BluetoothGattServerBinderServer> weak_ptr_to_this(this);
  auto callback = [=](bluetooth::GATTError error) {
    auto sp_to_this = weak_ptr_to_this.promote();
    if (!sp_to_this.get()) {
      VLOG(2) << "BluetoothLowEnergyBinderServer was deleted";
      return;
    }

    std::lock_guard<std::mutex> lock(*maps_lock());

    auto gatt_cb = GetGattServerCallback(server_id);
    if (!gatt_cb.get()) {
      VLOG(2) << "The callback was deleted";
      return;
    }

    gatt_cb->OnNotificationSent(device_address, error);
  };

  if (!gatt_server->SendNotification(
          std::string(String8(device_address).string()), characteristic_id,
          confirm, value, callback)) {
    LOG(ERROR) << "Failed to send notification";
    *_aidl_return = false;
    return Status::ok();
  }

  *_aidl_return = true;
  return Status::ok();
}

void BluetoothGattServerBinderServer::OnCharacteristicReadRequest(
    bluetooth::GattServer* gatt_server, const std::string& device_address,
    int request_id, int offset, bool is_long,
    const bluetooth::GattIdentifier& characteristic_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnCharacteristicReadRequest(
      String16(device_address.c_str(), device_address.length()), request_id,
      offset, is_long, characteristic_id);
}

void BluetoothGattServerBinderServer::OnDescriptorReadRequest(
    bluetooth::GattServer* gatt_server, const std::string& device_address,
    int request_id, int offset, bool is_long,
    const bluetooth::GattIdentifier& descriptor_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnDescriptorReadRequest(
      String16(device_address.c_str(), device_address.length()), request_id,
      offset, is_long, descriptor_id);
}

android::sp<IBluetoothGattServerCallback>
BluetoothGattServerBinderServer::GetGattServerCallback(int server_id) {
  auto cb = GetCallback(server_id);
  return android::sp<IBluetoothGattServerCallback>(
      static_cast<IBluetoothGattServerCallback*>(cb.get()));
}

std::shared_ptr<bluetooth::GattServer>
BluetoothGattServerBinderServer::GetGattServer(int server_id) {
  return std::static_pointer_cast<bluetooth::GattServer>(
      GetInstance(server_id));
}

void BluetoothGattServerBinderServer::OnRegisterInstanceImpl(
    bluetooth::BLEStatus status, android::sp<IInterface> callback,
    bluetooth::BluetoothInstance* instance) {
  VLOG(1) << __func__ << " instance ID: " << instance->GetInstanceId()
          << " status: " << status;
  bluetooth::GattServer* gatt_server =
      static_cast<bluetooth::GattServer*>(instance);
  gatt_server->SetDelegate(this);

  android::sp<IBluetoothGattServerCallback> cb(
      static_cast<IBluetoothGattServerCallback*>(callback.get()));
  cb->OnServerRegistered(status, (status == bluetooth::BLE_STATUS_SUCCESS)
                                     ? instance->GetInstanceId()
                                     : kInvalidInstanceId);
}

void BluetoothGattServerBinderServer::OnCharacteristicWriteRequest(
    bluetooth::GattServer* gatt_server, const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& characteristic_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnCharacteristicWriteRequest(
      String16(device_address.c_str(), device_address.length()), request_id,
      offset, is_prepare_write, need_response, value, characteristic_id);
}

void BluetoothGattServerBinderServer::OnDescriptorWriteRequest(
    bluetooth::GattServer* gatt_server, const std::string& device_address,
    int request_id, int offset, bool is_prepare_write, bool need_response,
    const std::vector<uint8_t>& value,
    const bluetooth::GattIdentifier& descriptor_id) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnDescriptorWriteRequest(
      String16(device_address.c_str(), device_address.length()), request_id,
      offset, is_prepare_write, need_response, value, descriptor_id);
}

void BluetoothGattServerBinderServer::OnExecuteWriteRequest(
    bluetooth::GattServer* gatt_server, const std::string& device_address,
    int request_id, bool is_execute) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnExecuteWriteRequest(
      String16(device_address.c_str(), device_address.length()), request_id,
      is_execute);
}

void BluetoothGattServerBinderServer::OnConnectionStateChanged(
    bluetooth::GattServer* gatt_server, const std::string& device_address, bool connected) {
  VLOG(2) << __func__;
  std::lock_guard<std::mutex> lock(*maps_lock());

  auto gatt_cb = GetGattServerCallback(gatt_server->GetInstanceId());
  if (!gatt_cb.get()) {
    LOG(WARNING) << "Callback for this GattServer was deleted.";
    return;
  }

  gatt_cb->OnConnectionStateChanged(
      String16(device_address.c_str(), device_address.length()), connected);
}

}  // namespace binder
}  // namespace ipc
