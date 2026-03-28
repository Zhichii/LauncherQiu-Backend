#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>

#include "instance.hpp"

class Minecraft {
    std::filesystem::path _path;
public:
    Minecraft(std::filesystem::path directory) {
    }
};

int main() {
    /* 画饼，驱动自己前进：
    try {
        Minecraft minecraft { "/mnt/80E8EEBFE8EEB298/Minecraft" };
        // 读取目录versions，每个folder，读取json，每个json，解析。
        std::vector<Instance>& instances = minecraft.instanceList();
        for (instance : instances) {
            printf("%s (%s) - ", instance.name().c_str(), instance.version().c_str());
            ModLoaderConfig& ders = instance.modLoaders();
            bool modded = false; // 不仅能用来判断是不是原版，还能判断是不是第一个，便于添加逗号。
            if (mod_loader.fabric().state()) {
                printf("Fabric %s", mod_loader.fabric().version());
                modded = true;
            }
            if (mod_loader.forge().state()) {
                if (!modded) printf(", ");
                printf("Forge %s", mod_loader.forge().version());
                modded = true;
            }
            if (mod_loader.neoforge().state()) {
                if (!modded) printf(", ");
                printf("NeoForge %s", mod_loader.neoforge().version());
                modded = true;
            }
            if (mod_loader.optifine().state()) {
                if (!modded) printf(", ");
                printf("OptiFine %s", mod_loader.optifine().version());
                modded = true;
            }
        }
    }
    except (std::exception e) {
        std::cout << e << std::endl;
    }
    */

    Instance inst("/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft", "1.18.2-Forge-OptiFine");
    InstanceContext context(1618, 1000, 5120);
    std::vector<Instance::Rule::Feature> features = {{"has_custom_resolution", true}};
    AccountsManager accounts_manager;
    JavaManager java_manager;
    std::string output = inst.generateLaunchCommand(context, accounts_manager, java_manager, features);
    std::filesystem::path test_sh = "/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft/testlaunch.sh";
    std::ofstream ofs;
    ofs.open(test_sh);
    ofs.write(output.data(), output.size());
    ofs.close();

};