#include "CommonLinuxApiSystem.h"
#include "ApiSystem.h"
#include "Log.h"
#include <functional>
#include <memory>
#include <regex>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <sdbus-c++/sdbus-c++.h>

CommonLinuxApiSystem::CommonLinuxApiSystem() {}

bool CommonLinuxApiSystem::isScriptingSupported(ScriptId script) {
    switch (script) {
        case ApiSystem::UPGRADE:
            return true;
        case ApiSystem::BLUETOOTH:
            return true;
        default:
            return ApiSystem::isScriptingSupported(script);
    }
}

std::pair<std::string, int> CommonLinuxApiSystem::updateSystem(
    const std::function<void(const std::string)>& func
) {
    const std::string command = "pkcon -p update -y --only-download && pkcon offline-get-prepared && pkcon offline-trigger";
    LOG(LogInfo) << "Starting system update using PackageKit CLI...";

    auto result = ApiSystem::executeScript(command, func);
    std::string lastLine = result.first;
    int returnCode = result.second;

    if (returnCode == 5) {
        returnCode = 0;
    }

    if (returnCode != 0) {
        const std::string errorMsg = "Update command failed with exit code: " + std::to_string(returnCode);
        LOG(LogError) << errorMsg;
        LOG(LogError) << "Command output: " << lastLine;
        return std::pair<std::string, int>(errorMsg, returnCode);
    }

    LOG(LogInfo) << "System update completed successfully.";
    return std::pair<std::string, int>(lastLine, returnCode);
}

bool CommonLinuxApiSystem::canUpdate(std::vector<std::string>& output) {
    LOG(LogInfo) << "Checking for updates using PackageKit CLI...";

    // Step 1: Check for offline updates
    const std::string offlineCommand = "LANG=C pkcon -p offline-get-prepared";

    bool offlineUpdatesPrepared = true;
    auto offlineCallback = [&output, &offlineUpdatesPrepared](const std::string& line) {
        if (line.find("No offline updates have been prepared") != std::string::npos) {
            offlineUpdatesPrepared = false;
        }
    };

    auto result = ApiSystem::executeScript(offlineCommand, offlineCallback);
    std::string lastLine = result.first;
    int returnCode = result.second;

    if (returnCode == 4 || returnCode == 5) {
        returnCode = 0;
    }

    if (returnCode != 0) {
        LOG(LogError) << "Offline command returned non-zero exit code: " << returnCode;
        LOG(LogError) << "Offline command output: " << lastLine;
        return false;
    }

    if (offlineUpdatesPrepared) {
        LOG(LogInfo) << "Offline updates have been prepared, skipping further update check.";
        return false;
    }

    // Step 2: Check for updates
    const std::string command = "pkcon -p refresh && LANG=C pkcon -p get-updates";

    bool updatesAvailable = false;
    auto updateCallback = [&output, &updatesAvailable](const std::string& line) {
        if (line.find("no updates available") == std::string::npos) {
            updatesAvailable = true;
        }
    };

    result = ApiSystem::executeScript(command, updateCallback);
    lastLine = result.first;
    returnCode = result.second;

    if (returnCode != 0) {
        LOG(LogError) << "Command returned non-zero exit code: " << returnCode;
        LOG(LogError) << "Command output: " << lastLine;
        return false;
    }

    if (updatesAvailable) {
        LOG(LogInfo) << "Updates are available.";
    } else {
        LOG(LogInfo) << "No updates found.";
    }

    return updatesAvailable;
}

bool CommonLinuxApiSystem::enableBluetooth() {
    // return executeScript("bluetoothctl power on");
    auto proxy = sdbus::createProxy("org.bluez", "/org/bluez/hci0");
    proxy->callMethod("Set")
                .onInterface("org.freedesktop.DBus.Properties")
                .withArguments("org.bluez.Adapter1", "Powered", sdbus::Variant(true));
    return true;
}

bool CommonLinuxApiSystem::disableBluetooth() {
    // return executeScript("bluetoothctl power off");
    auto proxy = sdbus::createProxy("org.bluez", "/org/bluez/hci0");
    proxy->callMethod("Set")
                .onInterface("org.freedesktop.DBus.Properties")
                .withArguments("org.bluez.Adapter1", "Powered", sdbus::Variant(false));
    return true;
}

struct Device {
    std::string address;
    std::string name;
};

Device extractDevice(const std::map<std::string, sdbus::Variant>& properties) {
    std::string address;
    std::string name;

    auto addressIt = properties.find("Address");
    if (addressIt != properties.end()) {
        address = addressIt->second.get<std::string>();
    }

    auto nameIt = properties.find("Name");
    if (nameIt != properties.end()) {
        name = nameIt->second.get<std::string>();
    }

    return Device{
        .address =  address,
        .name =  name,
    };
}

std::string deviceAddedEvent(const std::map<std::string, sdbus::Variant>& properties) {
    auto device = extractDevice(properties);
    std::ostringstream xml;
    xml << "<device id=\"" << device.address
        << "\" name=\"" << device.name
        << "\" status=\"added\" type=\"device\" />";
    return xml.str();
}

std::string deviceRemovedEvent(const std::map<std::string, sdbus::Variant>& properties) {
    auto device = extractDevice(properties);
    std::ostringstream xml;
    xml << "<device id=\"" << device.address
        << "\" name=\"" << device.name
        << "\" status=\"removed\" type=\"device\" />";
    return xml.str();
}

std::string deviceEvent(const std::map<std::string, sdbus::Variant>& properties) {
    auto device = extractDevice(properties);
    std::ostringstream xml;

    bool isConnected;
    auto connectedIt = properties.find("Connected");
    if (connectedIt != properties.end()) {
        isConnected = connectedIt->second.get<bool>();
    }

    uint32_t devClass;
    auto devClassIt = properties.find("Class");
    if (devClassIt != properties.end()) {
        devClass = devClassIt->second.get<uint32_t>();
    }

    std::string devType;
    if (devClass == 0x002508 || devClass == 0x000508) {
        devType = "joystick";
    }

    xml << "<device id=\"" << device.address
        << "\" name=\"" << device.name
        << "\" type=\"" << devType
        << "\" connected=\"" << (isConnected ? "yes" : "no")
        << "\" />";

    return xml.str();
}

void CommonLinuxApiSystem::startBluetoothLiveDevices(const std::function<void(const std::string)>& func) {
    sdbus::createProxy("org.bluez", "/org/bluez/hci0")
        ->callMethod("StartDiscovery")
        .onInterface("org.bluez.Adapter1");

    std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> pairedDevices;
    sdbus::createProxy("org.bluez", "/")
        ->callMethod("GetManagedObjects")
        .onInterface("org.freedesktop.DBus.ObjectManager")
        .storeResultsTo(pairedDevices);

    for (const auto& [devicePath, interfaces] : pairedDevices) {
        if (interfaces.count("org.bluez.Device1") > 0) {
            auto properties = pairedDevices[devicePath]["org.bluez.Device1"];
            if (!properties["Connected"].get<bool>()) {
                func(deviceAddedEvent(properties));
            }
        }
    }

    rootAdapterProxy = sdbus::createProxy(
        "org.bluez",
        "/"
    );

    rootAdapterProxy->
        uponSignal("InterfacesAdded")
        .onInterface("org.freedesktop.DBus.ObjectManager")
        .call([this, func](const sdbus::ObjectPath& objectPath, const std::map<std::string, std::map<std::string, sdbus::Variant>>& interfacesAndProperties){
            for (const auto& [interface, properties] : interfacesAndProperties) {
                if (interface == "org.bluez.Device1") {
                    func(deviceAddedEvent(properties));
                }
            }
        });

    rootAdapterProxy->
        uponSignal("InterfacesRemoved")
        .onInterface("org.freedesktop.DBus.ObjectManager")
        .call([this, func](const sdbus::ObjectPath& objectPath, const std::map<std::string, std::map<std::string, sdbus::Variant>>& interfacesAndProperties){
            for (const auto& [interface, properties] : interfacesAndProperties) {
                if (interface == "org.bluez.Device1") {
                    func(deviceRemovedEvent(properties));
                }
            }
        });

    rootAdapterProxy->finishRegistration();
}

void CommonLinuxApiSystem::stopBluetoothLiveDevices() {
    rootAdapterProxy.reset();

    try {
        auto proxy = sdbus::createProxy("org.bluez", "/org/bluez/hci0");
        proxy->callMethod("StopDiscovery").onInterface("org.bluez.Adapter1");
    } catch (const sdbus::Error& e) {
        // Ignore error
    }
}

bool CommonLinuxApiSystem::pairBluetoothDevice(const std::string& deviceName) {
    return (
        executeScript("bluetoothctl pair " + deviceName) &&
        executeScript("bluetoothctl trust " + deviceName)
    );
}

bool CommonLinuxApiSystem::connectBluetoothDevice(const std::string& deviceName) {
    return executeScript("bluetoothctl connect " + deviceName);
}

bool CommonLinuxApiSystem::disconnectBluetoothDevice(const std::string& deviceName) {
    return executeScript("bluetoothctl disconnect " + deviceName);
}

bool CommonLinuxApiSystem::removeBluetoothDevice(const std::string& deviceName) {
    return executeScript("bluetoothctl remove " + deviceName);
}


std::vector<std::string> CommonLinuxApiSystem::getPairedBluetoothDeviceList() {
    std::vector<std::string> devices;

    try {
        auto connection = sdbus::createSystemBusConnection();
        auto rootProxy = sdbus::createProxy(*connection, "org.bluez", "/");

        std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> pairedDevices;
        rootProxy->callMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .storeResultsTo(pairedDevices);

        for (const auto& [devicePath, interfaces] : pairedDevices) {
            if (interfaces.count("org.bluez.Device1") > 0) {
                auto properties = pairedDevices[devicePath]["org.bluez.Device1"];
                if (properties["Paired"].get<bool>()) {
                    devices.emplace_back(deviceEvent(properties));
                }
            }
        }
    } catch (const sdbus::Error& e) {
        LOG(LogError) << "D-Bus Error: " << e.what();
    }

    return devices;
}

bool CommonLinuxApiSystem::forgetBluetoothControllers() {
    return false;
}
