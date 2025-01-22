#if EXPERIMENTAL_COMMON_LINUX_API_SYSTEM
#pragma once

#include <memory>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/sdbus-c++.h>
#include "ApiSystem.h"

class CommonLinuxApiSystem : public ApiSystem
{
private:
    std::unique_ptr<sdbus::IProxy> bluetoothAdapterProxy;
    std::unique_ptr<sdbus::IProxy> rootAdapterProxy;
public:
    CommonLinuxApiSystem();
    virtual void deinit() override {};

    bool isScriptingSupported(ScriptId script) override;

    std::pair<std::string, int> updateSystem(const std::function<void(const std::string)>& func) override;
    bool canUpdate(std::vector<std::string>& output) override;

    // BLUETOOTH
    bool enableBluetooth() override;
    bool disableBluetooth() override;
    void startBluetoothLiveDevices(const std::function<void(const std::string)>& func) override;
    void stopBluetoothLiveDevices() override;
    bool pairBluetoothDevice(const std::string& deviceName) override;
    bool connectBluetoothDevice(const std::string& deviceName) override;
    bool disconnectBluetoothDevice(const std::string& deviceName) override;
    bool removeBluetoothDevice(const std::string& deviceName) override;
    std::vector<std::string> getPairedBluetoothDeviceList() override;
    bool forgetBluetoothControllers() override;
};
#endif
