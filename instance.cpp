#include "instance.hpp"

#include <fstream>
#include <minizip/unzip.h>
#include "config.hpp"
#include "strings.hpp"

Instance::File::File()  {
    this->_url = "";
    this->_sha1 = "";
    this->_size = 0;
}

Instance::File::File(Json::Value json) {
    this->_url = json["url"].asString();
    this->_sha1 = json["sha1"].asString();
    this->_size = json["size"].asLargestUInt(); // 我觉得应该和size_t一样。
    if (json.isMember("path")) {
        this->_path = json["path"].asString();
    }
}

std::string Instance::File::url() const { return this->_url; }

std::string Instance::File::sha1() const { return this->_sha1; }

size_t Instance::File::size() const { return this->_size; }

std::string Instance::File::path() const { return this->_url; }

Instance::Rule::Feature::Feature(std::string name, bool value) {
    this->_name = name;
    this->_value = value;
}

std::string Instance::Rule::Feature::name() const { return this->_name; }

bool Instance::Rule::Feature::value() const { return this->_value; }

Instance::Rule::Rule() {
    this->_action = true;
    this->_os_name = "";
    this->_os_version = "";
    this->_features = {};
}

Instance::Rule::Rule(Json::Value json) {
    this->_action = (json["action"] == "allow") && (json["action"] != "disallow");
    if (json.isMember("os")) {
        if (json["os"].isMember("name")) {
            this->_os_name = json["os"]["name"].asString();
        }
        if (json["os"].isMember("version")) {
            this->_os_version = json["os"]["version"].asString();
        }
    }
    if (json.isMember("features")) {
        for (const std::string& i : json["features"].getMemberNames()) {
            _features[i] = json["features"][i].asBool();
        }
    }
}

bool Instance::Rule::isAllow(std::vector<Feature> features) const {
    bool allow = 0;
    if(this->_os_name == OS_NAME || this->_os_version == "") allow = 1;
    if(!this->_action) allow = !allow;
    for (auto& i : this->_features) {
        bool found = false;
        bool v = false;
        for (auto& j : features) {
            if (j.name() == i.first) {
                found = true;
                v = j.value();
                break;
            }
        }
        if (!found) allow &= !i.second;
        else if (v) allow &= i.second;
        else allow &= !i.second;
    }
    return allow;
}

Instance::LibraryItem::LibraryItem(Json::Value json) {
    this->_name = json["name"].asString();
    if (json.isMember("rules")) {
        for (const auto& i : json["rules"]) {
            this->_rules.push_back(i);
        }
    }
    // 这个natives貌似新版本没有了？不知道。
    if (json.isMember("natives")) {
        std::vector<std::string> n = json["natives"].getMemberNames();
        for (const std::string& i : n) {
            this->_natives[i] = json["natives"][i].asString();
        }
    }
    if (json.isMember("downloads")) {
        if (json["downloads"].isMember("artifact")) {
            this->_artifact = json["downloads"]["artifact"];
        }
        // 这个classifiers貌似新版本没有了？不知道。
        if (json["downloads"].isMember("classifiers")) {
            for (const auto& i : this->_natives) {
                this->_classifiers[i.second] = json["downloads"]["classifiers"][i.second];
            }
        }
    }
}

bool Instance::LibraryItem::extractNatives(std::filesystem::path game_dir, std::string instance_name) {
    if (this->_natives.size()==0) return false;
    std::filesystem::path nativePath = game_dir / "libraries" / (this->_classifiers[this->_natives[OS_NAME]].path());
    std::filesystem::path extractPath = game_dir / "versions" / instance_name / (instance_name+"-natives");
    unzFile zipfile = unzOpen(nativePath.c_str());
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
    return true;
}

std::string Instance::LibraryItem::name() {
    std::vector<std::string> name_split;
    name_split = Strings::split(this->_name, ":");
    name_split.emplace(name_split.begin() + 2); // 这个代表的是版本……大概吧。
    return Strings::join(name_split, ":");
}



Instance::Instance(std::filesystem::path minecraft_path, std::string version) {
    if (std::filesystem::exists(minecraft_path / "versions" / version / (version+".json")) && std::filesystem::is_regular_file(minecraft_path / "versions" / version / (version+".json")));
    else throw std::runtime_error("manifest not found");
    std::ifstream ifs(minecraft_path / "versions" / version / (version+".json"));
    Json::CharReaderBuilder builder;
    Json::String errs;
    Json::Value root;
    Json::parseFromStream(builder, ifs, &root, &errs);
}

