#include "minecraft.hpp"

Minecraft::Minecraft(std::filesystem::path path) {
    _path = path;
    for (const auto& i : std::filesystem::directory_iterator(path / "versions")) {
        if (!i.is_directory()) continue;
        try {
            _instances.push_back(std::make_unique<Instance>(path, i.path().filename()));
        }
        catch (std::runtime_error e) {
            printf("Failed to load %s because:\n%s\n", i.path().filename().c_str(), e.what());
        }
    }
}

std::vector<std::weak_ptr<Instance>> Minecraft::instanceList() {
    std::vector<std::weak_ptr<Instance>> instance_list;
    for (auto& inst : _instances) {
        instance_list.push_back(inst);
    }
    return instance_list;
}