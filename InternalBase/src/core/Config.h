#pragma once
#include <string>
#include <vector>

namespace Config
{
    void Load(const std::string& name = "default");
    void Save(const std::string& name = "default");
    std::vector<std::string> ListConfigs();
    void DeleteConfig(const std::string& name);
}