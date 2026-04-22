#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>

#include "minecraft.hpp"
#include "strings.hpp"

int main() {
    // 只是为了测试，注释掉try。
    //try {
        std::weak_ptr<Instance> inst_w;
        Minecraft minecraft { "/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft" };
        // 读取目录versions，每个folder，读取json，每个json，解析。
        for (auto& instance : minecraft.instanceList()) {
            if (std::shared_ptr ptr = instance.lock()) {
                std::cout << ptr->instanceName();
                std::cout.flush();
                if (ptr->instanceName() == "Release") inst_w = instance;
                const Instance::Patches& patches = ptr->patches();
                if (patches.fabric().state() ||
                    patches.feather().state() ||
                    patches.neoforge().state() ||
                    patches.forge().state() ||
                    patches.optifine().state()) {
                    std::cout << " - ";
                }
                bool modded = false; // 不仅能用来判断是不是原版，还能判断是不是第一个，便于添加逗号。
                if (patches.fabric().state()) {
                    printf("Fabric %s", patches.fabric().version().c_str());
                    modded = true;
                }
                if (patches.feather().state()) {
                    if (modded) printf(", ");
                    printf("FeatherLoader %s", patches.feather().version().c_str());
                    modded = true;
                }
                if (patches.neoforge().state()) {
                    if (modded) printf(", ");
                    printf("NeoForge %s", patches.neoforge().version().c_str());
                    modded = true;
                }
                if (patches.forge().state()) {
                    if (modded) printf(", ");
                    printf("Forge %s", patches.forge().version().c_str());
                    modded = true;
                }
                if (patches.optifine().state()) {
                    if (modded) printf(", ");
                    printf("OptiFine %s", patches.optifine().version().c_str());
                    modded = true;
                }
                std::cout << std::endl;
            }
        }
    //}
    //catch (std::exception e) {
    //    std::cout << e.what() << std::endl;
    //}
return 0;
    InstanceContext context(1618, 1000, 5120);
    std::vector<Instance::Rule::Feature> features = {{"has_custom_resolution", true}};
    AccountsManager accounts_manager;
    JavaManager java_manager;
    std::vector<std::string> output;
    if (std::shared_ptr<Instance> inst = inst_w.lock()) {
        inst->generateLaunchArguments(output, context, accounts_manager, features);
        /*
        char** args = new char*[output.size()+1];
        for (size_t i = 0; i < output.size(); i++) args[i] = const_cast<char*>(output[i].c_str());
        args[output.size()] = nullptr;
        std::filesystem::path cwd = std::filesystem::current_path();
        std::filesystem::current_path(inst->minecraftPath());
        pid_t pid = fork();
        if (pid == 0) {
            std::cout << execvp(java_manager.find_java(inst->javaVersion()).c_str(), args) << std::endl;
            exit(1);
        }
        std::filesystem::current_path(cwd);
        */
        std::filesystem::path test_sh = "/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft/testlaunch.sh";
        std::ofstream ofs(test_sh, std::ios::out);
        ofs.write(java_manager.find_java(inst->javaVersion()).c_str(), java_manager.find_java(inst->javaVersion()).size());
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
    }
    return 0;
};