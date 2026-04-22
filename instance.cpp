#include "instance.hpp"

#include <fstream>
#include <jsoncpp/json/json.h>
#include <minizip/unzip.h>
#include <algorithm>
#include "config.hpp"
#include "strings.hpp"

InstanceContext::InstanceContext(size_t window_width, size_t window_height, size_t memory) {
    _window_width = window_width;
    _window_height = window_height;
    _memory = memory;
}

size_t InstanceContext::windowWidth() { return _window_width; }

size_t InstanceContext::windowHeight() { return _window_height; }

size_t InstanceContext::memory() { return _memory; }

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
    _os_name = "";
    _os_version = "";
    _os_arch = "";
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

void Instance::Rule::act(bool& allow, std::vector<Feature> features) const {
    // 我们需要判断是否处在范围内。
    if (_os_name != "" && _os_name != LAUNCHERQIU_OS_NAME && _os_name != "universal") return; // 不在范围内。
    if (_os_arch != LAUNCHERQIU_OS_ARCH && _os_arch != "") return; // 同样不在范围内。
    // 截至目前，我们仍处在被选择范围内。
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
        if (!found) return; // 我们很遗憾，但我们发现了一个未被支持的feature，故不在选择范围内。
        else if (v != i.second) return; // 该feature与要求相反，不处在范围内。
    }
    // 确信处在范围内。
    allow = _action;
}

Instance::LibraryItem::LibraryItem(Json::Value json) {
    _name = json["name"].asString();
    if (json.isMember("rules")) {
        for (const auto& i : json["rules"]) {
            _rules.push_back(i);
        }
    }
    else { // 手动认为allow。
        _rules.push_back(Rule());
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
    else if (true) {//Strings::count(_name, "native")) {
        native_path = instance._minecraft_path / "libraries" / (_artifact.path());
    }
    else return false;
    // TODO: Error detection
    unzFile zipfile = unzOpen(native_path.c_str());
    if (!zipfile) return false;
    int err;
    err = unzGoToFirstFile(zipfile);
    if (err != UNZ_OK) return false;
    do {
        unz_file_info info;
        err = unzGetCurrentFileInfo(zipfile, &info, nullptr, 0, nullptr, 0, nullptr, 0);
        if (err != UNZ_OK) return false;
        std::string name(info.size_filename, (char)0);
        std::string extra(info.size_file_extra, (char)0);
        std::string comment(info.size_file_comment, (char)0);
        err = unzGetCurrentFileInfo(zipfile, nullptr, name.data(), info.size_filename, extra.data(), info.size_file_extra, comment.data(), info.size_file_comment);
        if (err != UNZ_OK) return false;
        if ((LAUNCHERQIU_OS_NAME == "windows" && name.ends_with(".dll")) || ((LAUNCHERQIU_OS_NAME == "linux") && (name.ends_with(".so"))) || (LAUNCHERQIU_OS_NAME == "osx" && name.ends_with(".jnilib"))) {
            unzOpenCurrentFile(zipfile);
            char* buf = new char[info.uncompressed_size];
            int len = unzReadCurrentFile(zipfile, buf, info.uncompressed_size);
            std::ofstream ofs;
            ofs.open(extract_path / (std::filesystem::path(name).filename()), std::ios::binary);
            ofs.write(buf, len);
            delete[] buf;
            ofs.close();
            unzCloseCurrentFile(zipfile);
        }
    } while (unzGoToNextFile(zipfile) == UNZ_OK);
    unzClose(zipfile);
    return true;
}

std::string Instance::LibraryItem::name() const {
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

std::string Instance::LibraryItem::version() const {
    std::vector<std::string> name_split = Strings::split(_name, ":");
    if (name_split.size() >= 3) return name_split[2];
    else return "";
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
        if (name_split.size() < 3) printf("!?qiang gnaiq?!");
        std::string org = name_split[0];
        org = Strings::replace_all(org, ".", "/");
        std::string artifact = name_split[1];
        std::string version = name_split[2];
        path = path / org / artifact / version / (artifact+"-"+version+".jar");
    }
    return path;
}

bool Instance::LibraryItem::allow(std::vector<Rule::Feature> features) {
    bool allow = false;
    for (Rule& i : _rules) {
        i.act(allow, features);
    }
    return allow;
}

Instance::ArgumentItem::ArgumentItem(Json::Value json) {
    if (json.isString()) {
        _value = { json.asString() };
        _rules.push_back(Rule());
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
        else { // 手动认为allow。
            _rules.push_back(Rule());
        }
    }
}

bool Instance::ArgumentItem::allow(std::vector<Rule::Feature> features) const {
    bool allow = false;
    for (const Rule& i : _rules) {
        i.act(allow, features);
    }
    return allow;
}

std::vector<std::string> Instance::ArgumentItem::value() const { return _value; }

void Instance::init(const Json::Value& info) {
    // 我认为，无论发生什么错误，我们先用空值代替。
    // 因为有个东西叫_patches
    if (!info.isObject()) return; // 但这个实在没办法。
    if (info.isMember("id")) _id = info["id"].asString();
    if (info.isMember("mainClass")) _main_class = info["mainClass"].asString();
    if (info.isMember("assetIndex") && info["assetIndex"].isObject()) {
        _asset_index = File(info["assetIndex"]);
        if (info["assetIndex"].isMember("totalSize") && info["assetIndex"]["totalSize"].isInt()) _asset_index_total_size= info["assetIndex"]["totalSize"].asInt64();
        if (info["assetIndex"].isMember("id"))_asset_index_id = info["assetIndex"]["id"].asString();
    }
    if (info.isMember("complianceLevel") && info["complianceLevel"].isInt()) _compliance_level = info["complianceLevel"].asInt();
    if (info.isMember("javaVersion") && info["javaVersion"].isObject()) {
        if (info["javaVersion"].isMember("majorVersion") && info["javaVersion"]["majorVersion"].isInt()) _java_version = info["javaVersion"]["majorVersion"].asInt();
    }
    if (info.isMember("downloads") && info["downloads"].isObject()) {
        const Json::Value& downloads = info["downloads"];
        if (downloads.isMember("client_mappings") && downloads["client_mappings"].isObject()) _client_mappings = File(downloads["client_mappings"]);
        if (downloads.isMember("server_mappings") && downloads["client_mappings"].isObject()) _server_mappings = File(downloads["server_mappings"]);
        if (downloads.isMember("client")) _client = File(downloads["client"]);
        if (downloads.isMember("server")) _server = File(downloads["server"]);
    }
    if (info.isMember("logging") && info["loogging"].isObject()) {
        if (info["logging"].isMember("client") && info["loogging"]["client"].isObject()) {
            if (info["logging"]["client"].isMember("file") && info["logging"]["client"]["file"].isObject()) {
                _logging_file = File(info["logging"]["client"]["file"]);
                if (info["logging"]["client"]["file"].isMember("id")) _logging_id = info["logging"]["client"]["file"]["id"].asString();
                if (info["logging"]["client"]["file"].isMember("argument")) _logging_argument = info["logging"]["client"]["argument"].asString();
            }
        }
    }
    if (info.isMember("type")) _game_type = info["type"].asString();
    if (info.isMember("arguments")) {
        if (info["arguments"].isMember("jvm") && info["arguments"]["jvm"].isArray()) {
            for (auto argument : info["arguments"]["jvm"]) {
                _jvm_arguments.push_back(argument);
            }
        }
        if (info["arguments"].isMember("game") && info["arguments"]["game"].isArray()) {
            for (auto argument : info["arguments"]["game"]) {
                _game_arguments.push_back(argument);
            }
        }
    }
    if (info.isMember("minecraftArguments")) {
        std::vector<std::string> game_argument_list = Strings::split(info["minecraftArguments"].asString(), " ");
        for (auto argument : game_argument_list) {
            _game_arguments.push_back(Json::Value(argument));
        }
        _jvm_arguments = { Json::Value("-cp"), Json::Value("${classpath}") };
    }
    if (info.isMember("libraries") && info["libraries"].isArray()) {
        for (Json::Value i : info["libraries"]) {
            _libraries.push_back(i);
        }
    }
    // Patches可能是HMCL拓展
    if (info.isMember("patches") && info["patches"].isArray()) {
        for (const Json::Value& patch : info["patches"]) {
            _patches.push_back(std::make_unique<Instance>(patch));
        }
    }
    if (info.isMember("version")) _patch_version = info["version"].asString();
    else {
        _detected_patches = Patches(*this);
    }
    if (info.isMember("priority")) _patch_priority = info["priority"].asInt();
}

Instance::Instance(std::filesystem::path minecraft_path, std::string instance_name) {
    _minecraft_path = minecraft_path;
    _instance_name = instance_name;
    std::ifstream ifs(minecraft_path / "versions" / instance_name / (instance_name+".json"));
    if (!ifs) throw std::runtime_error(strerror(errno));
    Json::CharReaderBuilder builder;
    Json::String errs;
    Json::Value root;
    bool result = Json::parseFromStream(builder, ifs, &root, &errs);
    ifs.close();
    if (!result) throw std::runtime_error(errs);
    init(root);
}

Instance::Instance(const Json::Value& json) {
    init(json);
}

const std::filesystem::path& Instance::minecraftPath() { return _minecraft_path; }

const std::string& Instance::instanceName() { return _instance_name; }

const std::string& Instance::id() { return _id; }

const std::string& Instance::type() { return _game_type; }

int Instance::javaVersion() { return _java_version; }

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
    return Strings::join(library_list, LAUNCHERQIU_CLASSPATH_SEPARATOR);
}

void Instance::generateJVMArguments(std::vector<std::string>& output, const std::vector<Rule::Feature>& features, std::map<std::string,std::string>& jvm_values) {
    for (auto& i : _jvm_arguments) {
        if (i.allow(features)) {
            for (auto& j : i.value()) {
                std::string k = j;
                for (auto& pair : jvm_values) {
                    k = Strings::replace_all(k, pair.first, pair.second);
                }
                output.push_back(k);
            }
        }
    }
}

void Instance::generateGameArguments(std::vector<std::string>& output, const std::vector<Rule::Feature>& features, std::map<std::string, std::string>& game_values) {
    for (auto& i : _game_arguments) {
        if (i.allow(features)) {
            for (auto& j : i.value()) {
                std::string k = j;
                for (auto& pair : game_values) {
                    k = Strings::replace_all(k, pair.first, pair.second);
                }
                output.push_back(k);
            }
        }
    }
    // 处理一下窗口大小
    //if (!game_argument_list.contains()) {
    //    game_arguments += " --width ${resolution_width} --height ${resolution_height}";
    //}
}

void Instance::generateLaunchArguments(std::vector<std::string>& output, InstanceContext& context, AccountsManager& account_manager, const std::vector<Rule::Feature> features) {
    printf("Started geenerating launch arguments: %s, \"%s\".\n", _instance_name.c_str(), _minecraft_path.c_str());
    std::filesystem::path instance_path = "versions"; instance_path /= _instance_name;
    std::filesystem::path native_path = _minecraft_path / "versions" / _instance_name / (std::string("natives-")+LAUNCHERQIU_OS_NAME+"-"+LAUNCHERQIU_OS_ARCH);
    // 创建natives目录
    std::filesystem::create_directories(native_path);
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
    game_values["${auth_xuid}"] =           account_manager.userId();
    game_values["${clientId}"] =            account_manager.userId();
    game_values["${client_id}"] =           account_manager.userId();
    game_values["${clientid}"] =            account_manager.userId();
    game_values["${user_type}"] =           (account_manager.online()) ? ("msa") : ("legacy");
    game_values["${resolution_width}"] =    std::to_string(context.windowWidth());
    game_values["${resolution_height}"] =   std::to_string(context.windowHeight());
    game_values["${natives_directory}"] =   native_path;
    game_values["${user_properties}"] =     "{}";
    game_values["${classpath_separator}"] = LAUNCHERQIU_CLASSPATH_SEPARATOR;
    game_values["${library_directory}"] =   _minecraft_path / "libraries";
    std::map<std::string, std::string> jvm_values;
    jvm_values["${classpath}"] =            generateClassPath(features);
    jvm_values["${natives_directory}"] =    native_path;
    jvm_values["${launcher_name}"] =        "LauncherQiu";
    jvm_values["${launcher_version}"] =     "0.0.1";
    jvm_values["${version_name}"] =         _instance_name;
    jvm_values["${classpath_separator}"] =  LAUNCHERQIU_CLASSPATH_SEPARATOR;
    jvm_values["${library_directory}"] =    _minecraft_path / "libraries";
    if (_logging_argument != "") {
        output.push_back(Strings::replace_all(_logging_argument,
            "${path}", (_minecraft_path / "versions" / _instance_name / _logging_id).string()));
    }
    // 神秘硬编码参数……
    output.push_back("-Xmx"+std::to_string(context.memory())+"m");
    output.push_back("-XX:+UseG1GC");
    output.push_back("-XX:-UseAdaptiveSizePolicy");
    output.push_back("-XX:-OmitStackTraceInFastThrow");
    output.push_back("-Dlog4j2.formatMsgNoLookups=true");
    printf("Start generating JVM arguments.\n");
    generateJVMArguments(output, features, jvm_values);
    output.push_back(_main_class);
    printf("Start generating game arguments.\n");
    generateGameArguments(output, features, game_values);
    {
        bool flag = false;
        for (const auto& arg : output) {
            if (arg.starts_with("-Djava.library.path=")) {
                flag = true;
                break;
            }
        }
        if (!flag) {
            output.push_back("-Djava.library.path=" + native_path.string());
        }
    }
    printf("Launch command was successfully generated.\n");
}

// Fabric的

Instance::Patches::Fabric::Fabric() {}

Instance::Patches::Fabric::Fabric(std::string version) {
    auto v = Strings::split(version, ".");
    if (v.size() >= 1) _major = std::atoi(v[0].c_str());
    if (v.size() >= 2) _minor = std::atoi(v[1].c_str());
    if (v.size() >= 3) _patch = std::atoi(v[2].c_str());
}

unsigned short Instance::Patches::Fabric::major() const { return _major; }

unsigned short Instance::Patches::Fabric::minor() const { return _minor; }

unsigned short Instance::Patches::Fabric::patch() const { return _patch; }

std::string Instance::Patches::Fabric::version() const {
    std::ostringstream oss;
    oss << major() << "." << minor() << "." << patch();
    return oss.str();
}

bool Instance::Patches::Fabric::state() const { return !((_major == 65535) || (_minor == 65535) || (_patch == 65535)); }

// FeatherLoader的

Instance::Patches::Feather::Feather() {}

Instance::Patches::Feather::Feather(std::string version) {
    auto v = Strings::split(version, ".");
    if (v.size() >= 1) _a = std::atoi(v[0].c_str());
    if (v.size() >= 2) _b = std::atoi(v[1].c_str());
    if (v.size() >= 3) _c = std::atoi(v[2].c_str());
}

unsigned short Instance::Patches::Feather::a() const { return _a; }

unsigned short Instance::Patches::Feather::b() const { return _b; }

unsigned short Instance::Patches::Feather::c() const { return _c; }

std::string Instance::Patches::Feather::version() const {
    std::ostringstream oss;
    oss << a() << "." << b() << "." << c();
    return oss.str();
}

bool Instance::Patches::Feather::state() const { return !((_a == 65535) || (_b == 65535) || (_c == 65535)); }

// NeoForge的

Instance::Patches::NeoForge::NeoForge() {}

Instance::Patches::NeoForge::NeoForge(std::string version) {
    auto u = Strings::split(version, "-");
    if (u.size() >= 1){
        auto v = Strings::split(u[0], ".");
        if (v.size() >= 1) _major = std::atoi(v[0].c_str());
        if (v.size() >= 2) _minor = std::atoi(v[1].c_str());
        if (v.size() >= 3) _patch = std::atoi(v[2].c_str());
    }
    if (u.size() >= 2){
        if (u[1] == "beta") _beta = true;
    }
}

unsigned short Instance::Patches::NeoForge::major() const { return _major; }

unsigned short Instance::Patches::NeoForge::minor() const { return _minor; }

unsigned short Instance::Patches::NeoForge::patch() const { return _patch; }

bool Instance::Patches::NeoForge::beta() const { return _beta; }

std::string Instance::Patches::NeoForge::version() const {
    std::ostringstream oss;
    oss << major() << "." << minor() << "." << patch();
    if (beta()) oss << "-beta";
    return oss.str();
}

bool Instance::Patches::NeoForge::state() const { return !((_major == 65535) || (_minor == 65535) || (_patch == 65535)); }

// Forge的

Instance::Patches::Forge::Forge() {}

Instance::Patches::Forge::Forge(std::string version) {
    auto v = Strings::split(version, ".");
    if (v.size() >= 1) _major = std::atoi(v[0].c_str());
    if (v.size() >= 2) _minor = std::atoi(v[1].c_str());
    if (v.size() >= 3) _patch = std::atoi(v[2].c_str());
    if (v.size() >= 4) _small = std::atoi(v[3].c_str());
}

unsigned short Instance::Patches::Forge::major() const { return _major; }

unsigned short Instance::Patches::Forge::minor() const { return _minor; }

unsigned short Instance::Patches::Forge::patch() const { return _patch; }

unsigned short Instance::Patches::Forge::small() const { return _small; }

std::string Instance::Patches::Forge::version() const {
    std::ostringstream oss;
    oss << major() << "." << minor() << "." << patch();
    if (small() != 65535) oss << "." << small();
    return oss.str();
}

bool Instance::Patches::Forge::state() const { return !((_major == 65535) || (_minor == 65535) || (_patch == 65535)); }

// OptiFine的

Instance::Patches::OptiFine::OptiFine() {}

Instance::Patches::OptiFine::OptiFine(std::string version) {
    if (version.size() >= 6) _series = version[5];
    if (version.size() >= 7) _number = version[6] - '0';
    if (version.size() >= 12) _pre = version[11] - '0';
}

char Instance::Patches::OptiFine::series() const { return _series; }

unsigned short Instance::Patches::OptiFine::number() const { return _number; }

unsigned short Instance::Patches::OptiFine::pre() const { return _pre; }

std::string Instance::Patches::OptiFine::version() const {
    std::ostringstream oss;
    oss << "HD_U_" << series() << number();
    if (pre() != 65535) oss << "_pre" << pre();
    return oss.str();
}

bool Instance::Patches::OptiFine::state() const { return !((_series == 127) || (_number == 65535)); }

// 处理Patches

Instance::Patches::Patches() {}

Instance::Patches::Patches(const Instance& instance) {
    if (false) { //instance._patches.size()) {
        // 这个是HMCL的。如果有的话就以它为准了。
        for (auto& i : instance._patches) {
            if (i->id() == "fabric") {
                _fabric = Fabric(i->_patch_version);
            }
            if (i->id() == "neoforge") {
                _neoforge = NeoForge(i->_patch_version);
            }
            if (i->id() == "forge") {
                _forge = Forge(i->_patch_version);
            }
            if (i->id() == "optifine") {
                _optifine = OptiFine(i->_patch_version);
            }
        }
    }
    else {
        // 我们需要手动扫描库
        for (auto& library_item : instance._libraries) {
            if (library_item.name() == "net.fabricmc:fabric-loader") {
                _fabric = Fabric(library_item.version());
            }
            if (library_item.name() == "net.featherloader:feather-loader") {
                _feather = Feather(library_item.version());
            }
            if (library_item.name() == "net.minecraftforge:forge") {
                auto u = Strings::split(library_item.version(), "-");
                if (u.size() >= 2) {
                    _forge = Forge(u[1]);
                }
            }
            if (library_item.name() == "optifine:OptiFine" ||
                library_item.name() == "optifine:OptiFine:installer") {
                if (Strings::count(library_item.version(), "_") != 0) {
                    size_t idx = library_item.version().find("_")+1;
                    auto u = library_item.version().substr(idx);
                    _optifine = OptiFine(u);
                }
            }
        }
        // 或游戏参数
        std::string previous;
        for (auto& game_argument : instance._game_arguments) {
            if (previous == "--fml.neoForgeVersion") {
                if (game_argument.value().size() >= 1) {
                    _neoforge = NeoForge(game_argument.value()[0]);
                }
            }
            if (previous == "--fml.forgeVersion") {
                if (game_argument.value().size() >= 1) {
                    _forge = Forge(game_argument.value()[0]);
                }
            }
            if (game_argument.value().size() >= 1) {
                previous = game_argument.value()[0];
            }
        }
        // 或JVM参数
        for (auto& jvm_argument : instance._jvm_arguments) {
            if (jvm_argument.value().size() >= 1) {
                auto q = jvm_argument.value()[0];
                if (q.starts_with("-Dhmcl.transformer.candidates=${library_directory}/optifine/OptiFine")) {
                    auto p = q.substr(69);
                    if (Strings::count(p, "/") != 0) {
                        size_t idx = p.find('/');
                        auto r = p.substr(0, idx);
                        if (Strings::count(r, "_") != 0) {
                            size_t idx2 = r.find("_")+1;
                            auto u = r.substr(idx2);
                            _optifine = OptiFine(u);
                        }
                    }
                }
            }
        }
    }
}

const Instance::Patches::Fabric& Instance::Patches::fabric() const { return _fabric; }

const Instance::Patches::Feather& Instance::Patches::feather() const { return _feather; }

const Instance::Patches::NeoForge& Instance::Patches::neoforge() const { return _neoforge; }

const Instance::Patches::Forge& Instance::Patches::forge() const { return _forge; }

const Instance::Patches::OptiFine& Instance::Patches::optifine() const { return _optifine; }

const Instance::Patches& Instance::patches() {
    return _detected_patches;
}
