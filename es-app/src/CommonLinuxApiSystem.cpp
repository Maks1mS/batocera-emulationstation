#include "CommonLinuxApiSystem.h"
#include "Log.h"
#include <array>

CommonLinuxApiSystem::CommonLinuxApiSystem() {}

bool CommonLinuxApiSystem::isScriptingSupported(ScriptId script) {
    switch (script) {
        case ApiSystem::UPGRADE:
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
