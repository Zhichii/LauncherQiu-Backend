#include "instance.hpp"

#include <fstream>
#include <jsoncpp/json/json.h>
#include <minizip/unzip.h>
#include "config.hpp"
#include "strings.hpp"

Instance::File::File()  {
    _url = "";
    _sha1 = "";
    _size = 0;
}

Instance::File::File(Json::Value json) {
    _url = json["url"].asString();
    _sha1 = json["sha1"].asString();
    _size = json["size"].asLargestUInt(); // 我觉得应该和size_t一样。
    if (json.isMember("path")) {
        _path = json["path"].asString();
    }
}

std::string Instance::File::url() const { return _url; }

std::string Instance::File::sha1() const { return _sha1; }

size_t Instance::File::size() const { return _size; }

std::string Instance::File::path() const { return _path; }

Instance::Rule::Feature::Feature(std::string name, bool value) {
    _name = name;
    _value = value;
}

std::string Instance::Rule::Feature::name() const { return _name; }

bool Instance::Rule::Feature::value() const { return _value; }

Instance::Rule::Rule() {
    _action = true;
    _os_name = "unknown";
    _os_version = "";
    _os_arch = "x64";
    _features = {};
}

Instance::Rule::Rule(Json::Value json) {
    _action = (json["action"] == "allow");
    _os_name = "";
    _os_version = "";
    _os_arch = "";
    _features = {};
    if (json.isMember("os")) {
        if (json["os"].isMember("name")) {
            _os_name = json["os"]["name"].asString();
        }
        if (json["os"].isMember("version")) {
            _os_version = json["os"]["version"].asString();
        }
        if (json["os"].isMember("arch")) {
            _os_arch = json["os"]["arch"].asString();
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
    if (_os_name == LAUNCHERQIU_OS_NAME || _os_name == "universal" || _os_name == "") allow = 1;
    if (_os_arch == LAUNCHERQIU_OS_ARCH || _os_arch == "") allow &= 1;
    else allow &= 0;
    for (auto& i : _features) {
        bool found = false;
        bool v = false;
        for (auto& j : features) {
            if (j.name() == i.first) {
                found = true;
                v = j.value();
                break;
            }
        }
        if (!found) allow &= false;
        else if (v == i.second) allow &= true;
    }
    return _action? allow: !allow;
}

Instance::LibraryItem::LibraryItem(Json::Value json) {
    _name = json["name"].asString();
    if (json.isMember("rules")) {
        for (const auto& i : json["rules"]) {
            _rules.push_back(i);
        }
    }
    // 这个natives貌似新版本没有了？不知道。
    if (json.isMember("natives")) {
        std::vector<std::string> n = json["natives"].getMemberNames();
        for (const std::string& i : n) {
            _natives[i] = json["natives"][i].asString();
        }
    }
    if (json.isMember("downloads")) {
        if (json["downloads"].isMember("artifact")) {
            _artifact = json["downloads"]["artifact"];
        }
        // 这个classifiers貌似新版本没有了？不知道。
        if (json["downloads"].isMember("classifiers")) {
            for (const auto& i : _natives) {
                _classifiers[i.second] = json["downloads"]["classifiers"][i.second];
            }
        }
    }
}

bool Instance::LibraryItem::extractNatives(Instance& instance) {
    std::filesystem::path extract_path = instance._minecraft_path / "versions" / instance._instance_name / (std::string("natives-")+LAUNCHERQIU_OS_NAME+"-"+LAUNCHERQIU_OS_ARCH);
    std::filesystem::path native_path;
    if (_natives.size() != 0) {
        native_path = instance._minecraft_path / "libraries" / (_classifiers[_natives[LAUNCHERQIU_OS_NAME]].path());
    }
    else if (Strings::count(_name, "native")) {
        native_path = instance._minecraft_path / "libraries" / (_artifact.path());
    }
    else return false;
    // TODO: Error detection
    unzFile zipfile = unzOpen(native_path.c_str());
    unzGoToFirstFile(zipfile);
    do {
        unz_file_info info;
        char name[128] = {};
        char extra[4096] = {};
        char comment[4096] = {};
        unzGetCurrentFileInfo(zipfile, &info, name, 127, extra, 4095, comment, 4095);
        std::string name_s = name;
        if ((LAUNCHERQIU_OS_NAME == "windows" && name_s.ends_with(".dll")) || ((LAUNCHERQIU_OS_NAME == "linux") && (name_s.ends_with(".so"))) || (LAUNCHERQIU_OS_NAME == "osx" && name_s.ends_with(".jnilib"))) {
            unzOpenCurrentFile(zipfile);
            char* buf = new char[1048576];
            int len = unzReadCurrentFile(zipfile, buf, 1048575);
            std::ofstream ofs;
            ofs.open(extract_path / (std::filesystem::path(name_s).filename()));
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
    std::vector<std::string> name_split = Strings::split(_name, ":");
    std::string org = name_split[0];
    std::string artifact = name_split[1];
    //std::string version = name_split[2];
    if (name_split.size() >= 4) { // 有 classifier
        return org + ":" + artifact + ":" + name_split[3];
    } else { // 无 classifier
        return org + ":" + artifact;
    }
}

std::filesystem::path Instance::LibraryItem::finalLibPath(std::filesystem::path minecraft_path) {
    std::filesystem::path path = "libraries";
    if (_artifact.path() != "") {
        path = path / _artifact.path();
    }
    else if (_natives.size() > 0) {
        path = path / _classifiers[_natives[LAUNCHERQIU_OS_NAME]].path();
    }
    else { // 自己合成
        std::vector<std::string> name_split = Strings::split(_name, ":");
        // 认为name_split.size() >= 3的举手
        if (name_split.size() >= 3) printf("!?qiang gnaiq?!");
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
        _value = { json.asString() };
    }
    else {
        for (const auto& i : json["value"]) {
            _value.push_back(i.asString());
        }
        if (json.isMember("rules")) {
            for (const auto& i : json["rules"]) {
                _rules.push_back(i);
            }
        }
    }
}

bool Instance::ArgumentItem::allow(std::vector<Rule::Feature> features) {
    for (Rule& i : _rules) {
        if (!i.allow(features)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> Instance::ArgumentItem::value() { return _value; }

void Instance::init(Json::Value info) {
    _id = info["id"].asString();
    _main_class = info["mainClass"].asString();
    _asset_index = File(info["assetIndex"]);
    _asset_index_total_size= info["assetIndex"]["totalSize"].asInt64();
    _asset_index_id = info["assetIndex"]["id"].asString();
    _compliance_level= info["complianceLevel"].asInt();
    _java_version = info["javaVersion"]["majorVersion"].asInt();
    Json::Value downloads = info["downloads"];
    if (downloads.isMember("client_mappings")) {
        _client_mappings = File(downloads["client_mappings"]);
        _server_mappings = File(downloads["server_mappings"]);
    }
    _client = File(downloads["client"]);
    _server = File(downloads["server"]);
    if (info.isMember("logging")) {
        if (info["logging"].isMember("client")) {
            _logging_file = File(info["logging"]["client"]["file"]);
            _logging_id = info["logging"]["client"]["file"]["id"].asString();
            _logging_argument = info["logging"]["client"]["argument"].asString();
        }
    }
    else {
        _logging_file = {};
        _logging_id = {};
        _logging_argument = {};
    }
    _game_type = info["type"].asString();
    if (info.isMember("arguments")) {
        for (size_t i = 0; i < info["arguments"]["game"].size(); i ++) {
            _game_arguments.push_back(info["arguments"]["game"][(int)i]);
        }
        for (size_t i = 0; i < info["arguments"]["jvm"].size(); i++) {
            _jvm_arguments.push_back(info["arguments"]["jvm"][(int)i]);
        }
    }
    if (info.isMember("minecraftArguments")) {
        _legacy_game_arguments = info["minecraftArguments"].asString();
        _jvm_arguments = { Json::Value("-cp"), Json::Value("${classpath}") };
    }
    if (info.isMember("patches") &&
        (info["patches"].size() > 0)) {
        for (Json::Value& patch : info["patches"]) {
            _patches.push_back(std::make_unique<Instance>(patch));
        }
    }
    for (Json::Value i : info["libraries"]) {
        _libraries.push_back(i);
    }
}

Instance::Instance(std::filesystem::path minecraft_path, std::string instance_name) {
    _minecraft_path = minecraft_path;
    _instance_name = instance_name;
    if (std::filesystem::exists(minecraft_path / "versions" / instance_name / (instance_name+".json")) && std::filesystem::is_regular_file(minecraft_path / "versions" / instance_name / (instance_name+".json")));
    else throw std::runtime_error("manifest not found");
    std::ifstream ifs(minecraft_path / "versions" / instance_name / (instance_name+".json"));
    Json::CharReaderBuilder builder;
    Json::String errs;
    Json::Value root;
    Json::parseFromStream(builder, ifs, &root, &errs);
    init(root);
}

Instance::Instance(Json::Value& json) {
    init(json);
}

const std::filesystem::path& Instance::minecraftPath() { return _minecraft_path; }

const std::string& Instance::instanceName() { return _instance_name; }

const std::string& Instance::id() { return _id; }

const std::string& Instance::type() { return _game_type; }

std::string Instance::generateClassPath(const std::vector<Rule::Feature>& features) {
    std::map<std::string,std::string> available; // For solving libraries duplicating. 
    std::vector<std::string> library_list;
    for (auto& i : _libraries) {
        if (i.allow(features)) {
            if (i.extractNatives(*this)); // continue;
            available[i.name()] = i.finalLibPath(_minecraft_path);
        }
    }
    for (const auto& i : available) {
        library_list.push_back(i.second);
    }
    library_list.push_back(std::filesystem::path("versions") / _instance_name / (_instance_name + ".jar"));
    return Strings::join(library_list, ":");
}

std::string Instance::generateJVMArguments(const std::vector<Rule::Feature>& features, std::map<std::string,std::string>& jvm_values) {
    std::vector<std::string> jvm_argument_list;
    for (auto& i : _jvm_arguments) {
        if (i.allow(features)) {
            for (auto& j : i.value()) {
                std::string k = j;
                auto l = Strings::split(k, "=");
                for (auto& m : l) {
                    if (jvm_values.contains(m)) m = jvm_values[m];
                }
                k = Strings::join(l, "=");
                k = Strings::replace_all(k, "\\", "\\\\");
                k = Strings::replace_all(k, "\"", "\\\"");
                if (Strings::count(k, " ")) jvm_argument_list.push_back("\""+k+"\"");
                else jvm_argument_list.push_back(k);
            }
        }
    }
    return Strings::join(jvm_argument_list, " ");
}

std::string Instance::generateGameArguments(const std::vector<Rule::Feature>& features, std::map<std::string, std::string>& game_values) {
    std::string game_arguments;
    bool flag_optifine_forge = false;
    bool flag_forge = false;
    bool flag_optifine = false;
    bool flag_after_tweakclass = false;
    if (_game_arguments.size() > 0) {
        std::vector<std::string> game_argument_list;
        for (auto& i : _game_arguments) {
            if (i.allow(features)) {
                for (auto& j : i.value()) {
                    std::string k = j;
                    if (game_values.contains(k)) k = game_values[k];
                    if (k == "--tweakClass") {
                        flag_after_tweakclass = true;
                        continue;
                    }
                    if (flag_after_tweakclass && k == "net.minecraftforge.fml.common.launcher.FMLTweaker") {
                        //game_argument_list.push_back("--tweakClass");
                        // ……何意味，为啥我加了这个啊？
                        flag_forge = true;
                        flag_after_tweakclass = false;
                    }
                    if (flag_after_tweakclass && k == "optifine.OptiFineForgeTweaker") {
                        flag_optifine_forge = true;
                        flag_after_tweakclass = false;
                        continue;
                    }
                    if (flag_after_tweakclass && k == "optifine.OptiFineForgeTweaker") {
                        flag_optifine = true;
                        flag_after_tweakclass = false;
                        continue;
                    }
                    k = Strings::replace_all(k, "\\", "\\\\");
                    k = Strings::replace_all(k, "\"", "\\\"");
                    if (Strings::count(k, " ")) game_argument_list.push_back("\""+k+"\"");
                    else game_argument_list.push_back(k);
                }
            }
        }
        // 如果不这样额外处理OptiFine和Forge，貌似会崩溃
        if ((flag_optifine && flag_forge) || flag_optifine_forge) {
            game_argument_list.push_back("--tweakClass");
            game_argument_list.push_back("optifine.OptiFineForgeTweaker");
        }
        game_arguments = Strings::join(game_argument_list, " ");
    }
    else if (_legacy_game_arguments != "") {
        game_arguments = _legacy_game_arguments;
        // 处理一下窗口大小
        if (Strings::count(game_arguments, " --width") == 0) {
            game_arguments += " --width {resolution_width} --height {resolution_height}";
        }
        for (const auto& i : game_values) {
            std::string k = i.second;
            k = Strings::replace_all(k, "\\", "\\\\");
            k = Strings::replace_all(k, "\"", "\\\"");
            if (Strings::count(k, " ")) k = "\""+k+"\"";
            game_arguments = Strings::replace_all(game_arguments, i.first, k);
        }
        // 一样，特殊处理OptiFine和Forge，将--tweakClass放到后面去。
        if (Strings::count(game_arguments, " --tweakClass optifine.OptiFineForgeTweaker")) {
            game_arguments = Strings::replace_all(game_arguments, " --tweakClass optifine.OptiFineForgeTweaker", "");
            game_arguments += " --tweakClass optifine.OptiFineForgeTweaker";
        }
        else if (Strings::count(game_arguments, " --tweakClass net.minecraftforge.fml.common.launcher.FMLTweaker") != 0) {
            if (Strings::count(game_arguments, " --tweakClass optifine.OptiFineTweaker") != 0) {
                game_arguments = Strings::replace_all(game_arguments, " --tweakClass optifine.OptiFineTweaker", "");
                game_arguments += " --tweakClass optifine.OptiFineForgeTweaker";
            }
        }
    }
    else {
        return "";
    }
    return game_arguments;
}

InstanceContext::InstanceContext(size_t window_width, size_t window_height, size_t memory) {
    _window_width = window_width;
    _window_height = window_height;
    _memory = memory;
}

size_t InstanceContext::windowWidth() { return _window_width; }

size_t InstanceContext::windowHeight() { return _window_height; }

size_t InstanceContext::memory() { return _memory; }

std::string Instance::generateLaunchCommand(std::string& output, InstanceContext& context, AccountsManager& account_manager, JavaManager& java_manager, const std::vector<Rule::Feature> features) {
    //writeLog("Generating launching command: %s, \"%s\". ", getId().c_str(), gameDir.c_str());
    std::filesystem::path instance_path = "versions"; instance_path /= _instance_name;
    std::filesystem::path native_path = _minecraft_path / "versions" / _instance_name / (std::string("natives-")+LAUNCHERQIU_OS_NAME+"-"+LAUNCHERQIU_OS_ARCH);
    // 创建natives目录
    std::filesystem::create_directories(native_path);
    std::string java = java_manager.find_java(_java_version);
    //writeLog("Found Java \"%s\". ", finalJava.c_str());
    //writeLog("Reloging-in the account. ");
    account_manager.relogin();
    std::map<std::string, std::string> game_values;
    game_values["${version_name}"] =        _id;
    game_values["${game_directory}"] =      _minecraft_path;
    game_values["${assets_root}"] =         _minecraft_path / "assets";
    game_values["${assets_index_name}"] =   _asset_index_id;
    game_values["${version_type}"] =        _game_type;
    game_values["${auth_access_token}"] =   account_manager.userToken();
    game_values["${auth_session}"] =        account_manager.userToken();
    game_values["${auth_player_name}"] =    account_manager.userName();
    game_values["${auth_uuid}"] =           account_manager.userId();
    game_values["${clientId}"] =            account_manager.userId();
    game_values["${client_id}"] =           account_manager.userId();
    game_values["${user_type}"] =           (account_manager.online()) ? ("msa") : ("legacy");
    game_values["${resolution_width}"] =    std::to_string(context.windowWidth());
    game_values["${resolution_height}"] =   std::to_string(context.windowHeight());
    game_values["${natives_directory}"] =   native_path;
    game_values["${user_properties}"] =     "{}";
    game_values["${classpath_separator}"] = ":";
    game_values["${library_directory}"] =   _minecraft_path / "libraries\\";
    std::map<std::string, std::string> jvm_values;
    jvm_values["${classpath}"] =            generateClassPath(features);
    jvm_values["${natives_directory}"] =    native_path;
    jvm_values["${launcher_name}"] =        "LauncherQiu";
    jvm_values["${launcher_version}"] =     "0.0.1";
    //writeLog("Generating JVM arguments. ");
    std::string jvm_arguments = generateJVMArguments(features, jvm_values);
    //writeLog("Generating game arguments. ");
    std::string game_arguments = generateGameArguments(features, game_values);
    if (game_arguments == "") {
        //call({ "msgbx","error","minecraft.no_args","error" });
        //writeLog("Failed to generate game arguments. ");
        output = "";
        return "";
    }
    // 开始拼接参数！
    std::ostringstream oss;
    oss << "\"" << java << "\"";
    if (Strings::count(jvm_arguments, "-Djava.library.path") == 0) {
        oss << " \"-Djava.library.path=" << native_path.string() << "\"";
    }
    if (_logging_argument != "") {
        oss << " " << Strings::replace_all(_logging_argument,
            "${path}", "\"" + (_minecraft_path / "versions" / _instance_name / _logging_id).string() + "\"");
    }
    // 神秘硬编码参数……
    oss << " -Xmn" << std::to_string(context.memory()) << "m -XX:+UseG1GC -XX:-UseAdaptiveSizePolicy -XX:-OmitStackTraceInFastThrow -Dlog4j2.formatMsgNoLookups=true";
    oss << " " << jvm_arguments << " " << _main_class + " " + game_arguments;
    output = oss.str();
    return output;
}


