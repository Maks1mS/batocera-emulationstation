#if EXPERIMENTAL_COMMON_LINUX_API_SYSTEM
#pragma once

#include "ApiSystem.h"

class CommonLinuxApiSystem : public ApiSystem
{
public:
    CommonLinuxApiSystem();
    virtual void deinit() { };

    bool isScriptingSupported(ScriptId script) override;

    std::pair<std::string, int> updateSystem(const std::function<void(const std::string)>& func) override;
    bool canUpdate(std::vector<std::string>& output) override;
};
#endif
