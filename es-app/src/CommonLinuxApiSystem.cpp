#if EXPERIMENTAL_COMMON_LINUX_API_SYSTEM
#include "CommonLinuxApiSystem.h"
#include "ApiSystem.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include <functional>
#include <memory>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <sdbus-c++/sdbus-c++.h>

std::vector<
    std::pair<std::string, std::map<std::string, sdbus::Variant>>
> getBluetoothDevices(
    const std::function<bool(
        const std::map<std::string, sdbus::Variant> &
    )>&
        deviceFilter
) {
    std::vector<std::pair<std::string, std::map<std::string, sdbus::Variant>>> result;

    std::map<sdbus::ObjectPath,
            std::map<std::string, std::map<std::string, sdbus::Variant>>>
            devices;

    sdbus::createProxy(
        sdbus::createSystemBusConnection(),
        sdbus::ServiceName{"org.bluez"},
        sdbus::ObjectPath{"/"}
    )
            ->callMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .storeResultsTo(devices);

    for (const auto &[devicePath, interfaces] : devices) {
        if (interfaces.count("org.bluez.Device1") > 0) {
            const auto &properties = interfaces.at("org.bluez.Device1");
            if (auto nameIt = properties.find("Name"); nameIt != properties.end()) {
                if (auto name = nameIt->second.get<std::string>(); !name.empty()) {
                    if (deviceFilter(properties)) {
                        result.emplace_back(devicePath, properties);
                    }
                }
            }
        }
    }

    return result;
}

CommonLinuxApiSystem::CommonLinuxApiSystem() {}

bool CommonLinuxApiSystem::isScriptingSupported(ScriptId script) {
    switch (script) {
        case ApiSystem::UPGRADE:
        case ApiSystem::BLUETOOTH:
        case ApiSystem::WIFI:
        case ApiSystem::TIMEZONES:
        case ApiSystem::SERVICES:
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
    return executeScript("bluetoothctl power on");
}

bool CommonLinuxApiSystem::disableBluetooth() {
    return executeScript("bluetoothctl power off");
}

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

std::string deviceAddedEvent(const Device& device) {
    std::ostringstream xml;
    xml << "<device id=\"" << device.address
        << "\" name=\"" << device.name
        << "\" status=\"added\" type=\"device\" />";
    return xml.str();
}

std::string deviceRemovedEvent(const Device& device) {
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

bool pairBluetoothFilter(std::map<std::basic_string<char>, sdbus::Variant> properties) {
    return !properties["Paired"].get<bool>();
}

void CommonLinuxApiSystem::startBluetoothLiveDevices(const std::function<void(const std::string)>& func) {
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        discoveredDevicePaths.clear();
    }

    sdbus::createProxy(
        sdbus::createSystemBusConnection(),
        sdbus::ServiceName{"org.bluez"},
        sdbus::ObjectPath{"/org/bluez/hci0"}
    )
        ->callMethod("StartDiscovery")
        .onInterface("org.bluez.Adapter1");

    try {
        const auto devicesAndPaths = getBluetoothDevices([](const std::map<std::string, sdbus::Variant> &properties) {
            return !properties.at("Paired").get<bool>();
        });
        for (const auto& devicesAndPath : devicesAndPaths) {
            auto [path, deviceInfo] = devicesAndPath;
            auto device = extractDevice(deviceInfo);
            {
                std::lock_guard<std::mutex> lock(devicesMutex);
                if (discoveredDevicePaths.find(path) == discoveredDevicePaths.end()) {
                    discoveredDevicePaths[path] = device;
                    func(deviceAddedEvent(device));
                }
            }
        }
    } catch (const sdbus::Error& e) {
        LOG(LogError) << "D-Bus Error: " << e.what();
    }

    rootAdapterProxy = sdbus::createProxy(
        sdbus::createSystemBusConnection(),
        sdbus::ServiceName{"org.bluez"},
        sdbus::ObjectPath{"/"}
    );

    rootAdapterProxy->
        uponSignal("InterfacesAdded")
        .onInterface("org.freedesktop.DBus.ObjectManager")
        .call([this, func](const sdbus::ObjectPath& objectPath, const std::map<std::string, std::map<std::string, sdbus::Variant>>& interfacesAndProperties){
            for (const auto& [interface, properties] : interfacesAndProperties) {
                if (interface == "org.bluez.Device1") {
                    if (pairBluetoothFilter(properties)) {
                        auto device = extractDevice(properties);
                        {
                            std::lock_guard<std::mutex> lock(devicesMutex);
                            if (!device.name.empty() && discoveredDevicePaths.find(objectPath) == discoveredDevicePaths.end()) {
                                discoveredDevicePaths[objectPath] = device;
                                func(deviceAddedEvent(device));
                            }
                        }
                    }
                }
            }
        });

    rootAdapterProxy->
        uponSignal("InterfacesRemoved")
        .onInterface("org.freedesktop.DBus.ObjectManager")
        .call([this, func](const sdbus::ObjectPath& objectPath, const std::vector<std::string>& interfaces){
            for (const auto& interface : interfaces) {
                if (interface == "org.bluez.Device1") {
                    {
                        std::lock_guard<std::mutex> lock(devicesMutex);
                        if (discoveredDevicePaths.find(objectPath) != discoveredDevicePaths.end()) {
                            auto device = discoveredDevicePaths[objectPath];
                            discoveredDevicePaths.erase(objectPath);
                            func(deviceRemovedEvent(device));
                        }
                    }
                }
            }
        });
}

void CommonLinuxApiSystem::stopBluetoothLiveDevices() {
    rootAdapterProxy.reset();

    try {
        auto proxy = sdbus::createProxy(sdbus::createSystemBusConnection(), sdbus::ServiceName{"org.bluez"}, sdbus::ObjectPath{"/org/bluez/hci0"});
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
    std::vector<std::string> devicesXml;

    try {
        auto devices = getBluetoothDevices([](const std::map<std::string, sdbus::Variant> &properties) {
            return properties.at("Paired").get<bool>();
        });
        for (const auto& properties : devices) {
            devicesXml.emplace_back(deviceEvent(properties.second));
        }
    } catch (const sdbus::Error& e) {
        LOG(LogError) << "D-Bus Error: " << e.what();
    }

    return devicesXml;
}

bool CommonLinuxApiSystem::forgetBluetoothControllers() {
    try {
        auto devicesProps = getBluetoothDevices([](const std::map<std::string, sdbus::Variant> &properties) {
            return properties.at("Paired").get<bool>();
        });

        for (const auto& properties : devicesProps) {
            auto device = extractDevice(properties.second);
            executeScript("bluetoothctl remove " + device.address);
        }
    } catch (const sdbus::Error& e) {
        LOG(LogError) << "D-Bus Error: " << e.what();
    }

    return true;
}

bool CommonLinuxApiSystem::enableWifi(std::string ssid, std::string key) {
    LOG(LogDebug) << "disableWifi ";
    return executeScript("nmcli radio wifi on");
};
bool CommonLinuxApiSystem::disableWifi() {
    LOG(LogDebug) << "disableWifi ";
    return executeScript("nmcli radio wifi off");
};
std::vector<std::string> CommonLinuxApiSystem::getWifiNetworks(bool scan) {
    std::vector<std::string> res;
    return res;
};

std::vector<std::string> CommonLinuxApiSystem::getTimezones()
{
    LOG(LogDebug) << "ApiSystem::getTimezones";

    std::vector<std::string> ret = executeEnumerationScript("timedatectl list-timezones");
    std::sort(ret.begin(), ret.end());
    return ret;
}

std::string CommonLinuxApiSystem::getCurrentTimezone()
{
    LOG(LogInfo) << "ApiSystem::getCurrentTimezone";

    auto cmd = executeEnumerationScript("timedatectl show --property=Timezone --value");
    std::string tz = Utils::String::join(cmd, "");
    Utils::String::trim(tz);

    return tz;
}

bool CommonLinuxApiSystem::setTimezone(std::string tz)
{
    if (tz.empty())
        return false;

    return executeScript("timedatectl set-timezone \"" + tz + "\"");
}

std::vector<std::string> CommonLinuxApiSystem::getSystemInformations() {
    std::vector<std::string> info;

    info.push_back("Architecture: " + this->getRunningArchitecture());

    std::vector<std::string> memInfoLines = executeEnumerationScript("head /proc/meminfo");

    int memTotalKB = 0;
    int memAvailableKB = 0;

    for (const auto& line : memInfoLines) {
        if (Utils::String::startsWithIgnoreCase(line, "MemTotal:")) {
            std::vector<std::string> parts = Utils::String::split(line, ' ', true);
            if (parts.size() > 1) {
                memTotalKB = Utils::String::toInteger(parts[1]);
            }
        } else if (Utils::String::startsWithIgnoreCase(line, "MemAvailable:")) {
            std::vector<std::string> parts = Utils::String::split(line, ' ', true);
            if (parts.size() > 1) {
                memAvailableKB = Utils::String::toInteger(parts[1]);
            }
        }
    }

    int memTotalMB = memTotalKB / 1024;
    int memAvailableMB = memAvailableKB / 1024;

    info.push_back(Utils::String::format("Available Memory: %d/%d MB", memAvailableMB, memTotalMB));

    return info;
};

std::vector<Service> CommonLinuxApiSystem::getServices() {
    std::vector<Service> services;
    LOG(LogDebug) << "ApiSystem::getServices";

    auto slines = executeEnumerationScript("systemctl list-unit-files --type=service --no-pager");
    for (auto sline : slines)
    {
        auto splits = Utils::String::split(sline, ' ', true);
        if (splits.size() >= 2)
        {
            Service s;
            s.name = splits[0];
            s.enabled = (splits[1] == "enabled");
            services.push_back(s);
        }
    }

    return services;
};

bool CommonLinuxApiSystem::enableService(std::string name, bool enable) {
    std::string serviceName = name;
    if (serviceName.find(" ") != std::string::npos)
        serviceName = "\"" + serviceName + "\"";

    LOG(LogDebug) << "ApiSystem::enableService " << serviceName;

    bool res = executeScript("systemctl " + std::string(enable ? "enable" : "disable") + " " + serviceName);
    if (res)
        res = executeScript("systemctl " + std::string(enable ? "start" : "stop") + " " + serviceName);

    return res;
};

#endif
