#pragma once
#include <filesystem>
#include <jsoncpp/json/json.h>
#include "accounts.hpp"
#include "java.hpp"

class InstanceContext {
	size_t _window_width;
	size_t _window_height;
	size_t _memory;
public:
	InstanceContext(size_t window_width, size_t window_height, size_t memory);
	size_t windowWidth();
	size_t windowHeight();
	size_t memory(); // Megabytes
};

class Instance {
public:
    class File {
		std::string _url;
		std::string _sha1;
		size_t _size;
		std::string _path;
	public:
		File();
		File(Json::Value json);
        std::string url() const;
        std::string sha1() const;
        size_t size() const;
        std::string path() const;
	};
	class Rule {
		bool _action; // allow=1, disallow=0
		std::string _os_name;
		std::string _os_version;
		std::string _os_arch;
		std::map<std::string,bool> _features;
	public:
		class Feature {
			std::string _name;
			bool _value;
		public:
			Feature(std::string name, bool value);
			std::string name() const;
			bool value() const;
		};
		Rule();
		Rule(Json::Value json);
        void act(bool& allow, std::vector<Feature> features) const;
    };
	class LibraryItem {
		std::string _name;
		File _artifact;
		std::map<std::string,File> _classifiers;
		std::map<std::string,std::string> _natives;
		std::vector<Rule> _rules;
	public:
		LibraryItem(Json::Value json);
		bool extractNatives(Instance& instance);
		std::string name() const; // 和_name不一样的是，这个会去除版本，只保留真正的名字。
		std::string version() const;
		std::filesystem::path finalLibPath(std::filesystem::path minecraft_path);
		bool allow(std::vector<Rule::Feature> features);
	};
	class ArgumentItem {
		std::vector<std::string> _value;
		std::vector<Rule> _rules;
	public:
		ArgumentItem(Json::Value json);
		bool allow(std::vector<Rule::Feature> features) const;
		std::vector<std::string> value() const;
	};
	class Patches {
	public:
		class Fabric {
			unsigned short _major = 65535, _minor = 65535, _patch = 65535;
			unsigned short major() const;
			unsigned short minor() const;
			unsigned short patch() const;
		public:
			Fabric();
			Fabric(std::string version);
			std::string version() const;
			bool state() const;
		} _fabric;
		class Feather { // 我自己瞎写的模组加载器哦（至少实现了注入！）。感兴趣的可以联系我。
			unsigned short _a = 65535, _b = 65535, _c = 65535;
			unsigned short a() const;
			unsigned short b() const;
			unsigned short c() const;
		public:
			Feather();
			Feather(std::string version);
			std::string version() const;
			bool state() const;
		} _feather;
		class NeoForge {
			unsigned short _major = 65535, _minor = 65535, _patch = 65535;
			bool _beta = false;
			unsigned short major() const;
			unsigned short minor() const;
			unsigned short patch() const;
			bool beta() const;
		public:
			NeoForge();
			NeoForge(std::string version);
			std::string version() const;
			bool state() const;
		} _neoforge;
		class Forge {
			unsigned short _major = 65535, _minor = 65535, _patch = 65535, _small = 65535;
			unsigned short major() const;
			unsigned short minor() const;
			unsigned short patch() const;
			unsigned short small() const;
		public:
			Forge();
			Forge(std::string version);
			std::string version() const;
			bool state() const;
		} _forge;
		class OptiFine {
			char _series = 127;
			unsigned short _number = 65535;
			unsigned short _pre = 65535;
			char series() const;
			unsigned short number() const;
			unsigned short pre() const;
		public:
			OptiFine();
			OptiFine(std::string version);
			std::string version() const;
			bool state() const;
		} _optifine;
	private:
	public:
		Patches();
		Patches(const Instance& info);
		const Fabric& fabric() const;
		const Feather& feather() const;
		const NeoForge& neoforge() const;
		const Forge& forge() const;
		const OptiFine& optifine() const;
	};
private:
	std::filesystem::path _minecraft_path;
	std::string _instance_name;
	std::string _id;
	std::string _main_class;
	File _asset_index;
	long long _asset_index_total_size;
	std::string _asset_index_id;
	int _compliance_level;
	int _java_version;
	File _client;
	File _server;
	File _client_mappings;
	File _server_mappings;
	File _logging_file;
	std::string _logging_id;
	std::string _logging_argument;
	std::string _game_type;
	std::vector<LibraryItem> _libraries;
	std::vector<ArgumentItem> _game_arguments;
	std::vector<ArgumentItem> _jvm_arguments;
	std::string _patch_version; // _patches会有这项
	std::string _patch_priority; // _patches会有这项
	Patches _detected_patches;
	std::vector<std::unique_ptr<Instance>> _patches;
	void init(const Json::Value& info);
	std::string generateClassPath(const std::vector<Rule::Feature>& features);
	void generateJVMArguments(std::vector<std::string>& output, const std::vector<Rule::Feature>& features, std::map<std::string,std::string>& jvm_values);
	void generateGameArguments(std::vector<std::string>& output, const std::vector<Rule::Feature>& features, std::map<std::string, std::string>& game_values);
public:
    Instance(std::filesystem::path minecraft_path, std::string instance_name);
    Instance(const Json::Value& json);
	const std::filesystem::path& minecraftPath();
	const std::string& instanceName();
	const std::string& id();
	const std::string& type();
	int javaVersion();
	const Patches& patches();
	void generateLaunchArguments(std::vector<std::string>& output, InstanceContext& context, AccountsManager& account_manager, const std::vector<Rule::Feature> features);
};