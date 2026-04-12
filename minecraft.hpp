#pragma once

#include <filesystem>
#include "instance.hpp"

class Minecraft {
    std::filesystem::path _path;
    std::vector<std::shared_ptr<Instance>> _instances;
public:
    Minecraft(std::filesystem::path path);
    std::vector<std::weak_ptr<Instance>> instanceList();
};