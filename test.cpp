#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>

#include "minecraft.hpp"
#include "strings.hpp"

int main() {
    try {
        Minecraft minecraft { "/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft" };
        // 读取目录versions，每个folder，读取json，每个json，解析。
        for (auto& instance : minecraft.instanceList()) {
            if (std::shared_ptr ptr = instance.lock()) {
                std::cout << ptr->instanceName() << ", ";
                std::cout.flush();
            }
        }
        /* 画饼，驱动自己前进：
        for (instance : minecraft.instanceList()) {
            printf("%s (%s) - ", instance.name().c_str(), instance.version().c_str());
            ModLoaderConfig& mod_loader = instance.modLoaders();
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
        */
    }
    catch (std::exception e) {
        std::cout << e.what() << std::endl;
    }
    std::cout << std::endl;
return 0;
    Instance inst("/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft", "Release");
    InstanceContext context(1618, 1000, 5120);
    std::vector<Instance::Rule::Feature> features = {{"has_custom_resolution", true}};
    AccountsManager accounts_manager;
    JavaManager java_manager;
    std::vector<std::string> output;
    inst.generateLaunchArguments(output, context, accounts_manager, features);
return 0;
    char** args = new char*[output.size()+1];
    for (size_t i = 0; i < output.size(); i++) args[i] = const_cast<char*>(output[i].c_str());
    args[output.size()] = nullptr;
    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::current_path(inst.minecraftPath());
    pid_t pid = fork();
    if (pid == 0) {
        std::cout << execvp(java_manager.find_java(inst.javaVersion()).c_str(), args) << std::endl;
        exit(1);
    }
    std::filesystem::current_path(cwd);
return 0;
    std::filesystem::path test_sh = "/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft/testlaunch.sh";
    std::ofstream ofs(test_sh, std::ios::out);
    ofs.write(java_manager.find_java(inst.javaVersion()).c_str(), java_manager.find_java(inst.javaVersion()).size());
    ofs.write(" ", 1);
    for (auto& i : output) {
        std::string k = i;
        k = Strings::replace_all(k, "\\", "\\\\");
        k = Strings::replace_all(k, "\"", "\\\"");
        k = Strings::replace_all(k, "$", "\\$");
        k = Strings::replace_all(k, ";", "\\;");
        k = Strings::replace_all(k, " ", "\\ ");
        k = Strings::replace_all(k, "`", "\\`");
        k = Strings::replace_all(k, "!", "\\!");
        ofs.write(k.data(), k.size());
        ofs.write(" ", 1);
    }
    ofs.close();
    return 0;
};