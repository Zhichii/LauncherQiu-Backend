#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>

//#include "instance.hpp"
#include <minizip/unzip.h>
#include <minizip/mz_zip.h>
//#include <minizip/mz.h>

class Minecraft {
    std::filesystem::path _path;
public:
    Minecraft(std::filesystem::path directory) {
    }
};

int main() {
    /*
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

    //Instance inst("/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft", "Release-OptiFine");

    Json::Value v;

    std::filesystem::path temp = "/mnt/80E8EEBFE8EEB298/Minecraft/.minecraft/libraries/org/lwjgl/lwjgl-opengl/3.4.1/lwjgl-opengl-3.4.1-natives-linux.jar";
    //void *zip_file = mz_zip_create();
    unzFile zipfile = unzOpen(temp.c_str());
    unzGoToFirstFile(zipfile);
    while (unzGoToNextFile(zipfile) == UNZ_OK) {
        unz_file_info info;
        char name[128];
        char extra[4096];
        char comment[4096];
        unzGetCurrentFileInfo(zipfile, &info, name, 127, extra, 4095, comment, 4095);
        std::string name_s = name;
        if (name_s.ends_with(".lib") || name_s.ends_with(".so")) {
            unzOpenCurrentFile(zipfile);
            char* buf = new char[1048576];
            unzReadCurrentFile(zipfile, buf, 1048575);
            std::ofstream ofs;
            ofs.open(std::filesystem::path(name_s).filename());
            ofs.write(buf, unzTell(zipfile));
            delete buf;
            ofs.close();
            unzCloseCurrentFile(zipfile);
        }
    }
    unzClose(zipfile);
};