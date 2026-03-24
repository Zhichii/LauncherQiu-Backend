#include "instance.hpp"

#include <fstream>
#include <jsoncpp/json/json.h>
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

std::string Instance::File::path() const { return this->_path; }

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

bool Instance::Rule::allow(std::vector<Feature> features) const {
    bool allow = 0;
    if (this->_os_name == OS_NAME || this->_os_version == "") allow = 1;
    if (!this->_action) allow = !allow;
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

bool Instance::LibraryItem::extractNatives(std::filesystem::path minecraft_path, std::string instance_name) {
    if (this->_natives.size()==0) return false;
    std::filesystem::path nativePath = minecraft_path / "libraries" / (this->_classifiers[this->_natives[OS_NAME]].path());
    std::filesystem::path extractPath = minecraft_path / "versions" / instance_name / (instance_name+"-natives");
    // TODO: Error detection
    unzFile zipfile = unzOpen(nativePath.c_str());
    unzGoToFirstFile(zipfile);
    std::filesystem::create_directories(extractPath);
    do {
        unz_file_info info;
        char name[128];
        char extra[4096];
        char comment[4096];
        unzGetCurrentFileInfo(zipfile, &info, name, 127, extra, 4095, comment, 4095);
        std::string name_s = name;
        if (name_s.ends_with(".lib") || name_s.ends_with(".so")) {
            unzOpenCurrentFile(zipfile);
            char* buf = new char[1048576];
            int len = unzReadCurrentFile(zipfile, buf, 1048575);
            std::ofstream ofs;
            ofs.open(extractPath / (std::filesystem::path(name_s).filename()));
            ofs.write(buf, len);
            delete[] buf;
            ofs.close();
            unzCloseCurrentFile(zipfile);
        }
    } while (unzGoToNextFile(zipfile) == UNZ_OK);
    unzClose(zipfile);
    return true;
}

std::string Instance::LibraryItem::name() {
    std::vector<std::string> name_split = Strings::split(this->_name, ":");
    // 认为name_split.size()==3的举手
    if (name_split.size() != 3) printf("!?qiang gnaiq?!");
    std::string org = name_split[0];
    std::string artifact = name_split[1];
    //std::string version = name_split[2];
    return org + ":" + artifact;
}

std::filesystem::path Instance::LibraryItem::finalLibPath(std::filesystem::path minecraft_path) {
    std::filesystem::path path = minecraft_path / "libraries";
    if (this->_artifact.path() != "") {
        path = path / this->_artifact.path();
    }
    else if (this->_natives.size() > 0) {
        path = path / this->_classifiers[this->_natives[OS_NAME]].path();
    }
    else {
        std::vector<std::string> name_split = Strings::split(this->_name, ":");
        // 认为name_split.size()==3的举手
        if (name_split.size() != 3) printf("!?qiang gnaiq?!");
        std::string org = name_split[0];
        org = Strings::replace_all(org, ".", "/");
        std::string artifact = name_split[1];
        std::string version = name_split[2];
        path = path / org / artifact / version / (artifact+"-"+version+".jar");
    }
    return path;
}

bool Instance::LibraryItem::allow(std::vector<Rule::Feature> features) {
    for (Rule& i : _rules) {
        if (!i.allow(features)) {
            return false;
        }
    }
    return true;
}

Instance::ArgumentItem::ArgumentItem(Json::Value json) {
    if (json.isString()) {
        this->_value = { json.asString() };
    }
    else {
        for (const auto& i : json["value"]) {
            this->_value.push_back(i.asString());
        }
        if (json.isMember("rules")) {
            for (const auto& i : json["rules"]) {
                this->_rules.push_back(i);
            }
        }
    }
}

bool Instance::ArgumentItem::allow(std::vector<Rule::Feature> features) {
    for (Rule& i : this->_rules) {
        if (!i.allow(features)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> Instance::ArgumentItem::value() { return this->_value; }

void Instance::init(Json::Value info) {
    this->_id = info["id"].asString();
    this->_main_class = info["mainClass"].asString();
    this->_asset_index = File(info["assetIndex"]);
    this->_asset_index_total_size= info["assetIndex"]["totalSize"].asInt64();
    this->_asset_index_id = info["assetIndex"]["id"].asString();
    this->_compliance_level= info["complianceLevel"].asInt();
    this->_java_version = info["javaVersion"]["majorVersion"].asInt();
    Json::Value downloads = info["downloads"];
    if (downloads.isMember("client_mappings")) {
        this->_client_mappings = File(downloads["client_mappings"]);
        this->_server_mappings = File(downloads["server_mappings"]);
    }
    this->_client = File(downloads["client"]);
    this->_server = File(downloads["server"]);
    if (info.isMember("logging")) {
        if (info["logging"].isMember("client")) {
            this->_logging_file = File(info["logging"]["client"]["file"]);
            this->_logging_id = info["logging"]["client"]["file"]["id"].asString();
            this->_logging_argument = info["logging"]["client"]["argument"].asString();
        }
    }
    else {
        this->_logging_file = {};
        this->_logging_id = {};
        this->_logging_argument = {};
    }
    this->_game_type = info["type"].asString();
    if (info.isMember("arguments")) {
        for (size_t i = 0; i < info["arguments"]["game"].size(); i ++) {
            this->_game_arguments.push_back(info["arguments"]["game"][(int)i]);
        }
        for (size_t i = 0; i < info["arguments"]["jvm"].size(); i++) {
            this->_jvm_arguments.push_back(info["arguments"]["jvm"][(int)i]);
        }
    }
    if (info.isMember("minecraftArguments")) {
        this->_legacy_game_arguments = info["minecraftArguments"].asString();
        this->_jvm_arguments = { Json::Value("-cp"), Json::Value("${classpath}") };
    }
    if (info.isMember("patches") &&
        (info["patches"].size() > 0)) {
        for (Json::Value& patch : info["patches"]) {
            this->_patches.push_back(std::make_unique<Instance>(patch));
        }
    }
    for (Json::Value i : info["libraries"]) {
        this->_libraries.push_back(i);
    }
}

Instance::Instance(std::filesystem::path minecraft_path, std::string version) {
    if (std::filesystem::exists(minecraft_path / "versions" / version / (version+".json")) && std::filesystem::is_regular_file(minecraft_path / "versions" / version / (version+".json")));
    else throw std::runtime_error("manifest not found");
    std::ifstream ifs(minecraft_path / "versions" / version / (version+".json"));
    Json::CharReaderBuilder builder;
    Json::String errs;
    Json::Value root;
    Json::parseFromStream(builder, ifs, &root, &errs);
    this->init(root);
}

Instance::Instance(Json::Value& json) {
    this->init(json);
}
