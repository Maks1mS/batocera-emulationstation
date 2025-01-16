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

    std::array<char, 128> buffer;
    std::ostringstream result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        const std::string errorMsg = "Failed to execute update command.";
        LOG(LogError) << errorMsg;
        return std::pair<std::string, int>(errorMsg, -1);
    }

    std::string status;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        result << line;

        if (func) {
            func(line);
        }

        status = line;
    }

    int returnCode = WEXITSTATUS(pclose(pipe.release()));
    if (returnCode == 5) {
        returnCode = 0;
    }
    if (returnCode != 0) {
        const std::string errorMsg = "Update command failed with exit code: " + std::to_string(returnCode);
        LOG(LogError) << errorMsg;
        LOG(LogError) << "Command output: " << result.str();
        return std::pair<std::string, int>(errorMsg, returnCode);
    }

    LOG(LogInfo) << "System update completed successfully.";
    return std::pair<std::string, int>(status, returnCode);
}

bool CommonLinuxApiSystem::canUpdate(std::vector<std::string>& output) {
    LOG(LogInfo) << "Checking for updates using PackageKit CLI...";

    const std::string command = "LANG=C pkcon -p get-updates";

    std::array<char, 128> buffer;
    std::ostringstream result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        LOG(LogError) << "Failed to execute command: " << command;
        return false;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result << buffer.data();
    }

    int returnCode = pclose(pipe.release());
    if (returnCode != 0) {
        LOG(LogError) << "Command returned non-zero exit code: " << returnCode;
        LOG(LogError) << "Command output: " << result.str();
        return false;
    }

    std::string line;
    std::istringstream stream(result.str());
    bool updatesAvailable = true;

    while (std::getline(stream, line)) {
        // output.push_back(line);
        if (!updatesAvailable && line.find("no updates available") != std::string::npos) {
            updatesAvailable = false;
            // No need to continue parsing if updates are found
            break;
        }
    }

    if (updatesAvailable) {
        LOG(LogInfo) << "Updates are available.";
    } else {
        LOG(LogInfo) << "No updates found.";
    }

    return updatesAvailable;
}

