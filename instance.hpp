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
		std::string name(); // 和_name不一样的是，这个会去除版本，只保留真正的名字。
		std::filesystem::path finalLibPath(std::filesystem::path minecraft_path);
		bool allow(std::vector<Rule::Feature> features);
	};
	class ArgumentItem {
		std::vector<std::string> _value;
		std::vector<Rule> _rules;
	public:
		ArgumentItem(Json::Value json);
		bool allow(std::vector<Rule::Feature> features);
		std::vector<std::string> value();
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
	std::vector<std::unique_ptr<Instance>> _patches;
	void init(Json::Value info);
	std::string generateClassPath(const std::vector<Rule::Feature>& features);
	std::vector<std::string> generateJVMArguments(const std::vector<Rule::Feature>& features, std::map<std::string,std::string>& jvm_values);
	std::vector<std::string> generateGameArguments(const std::vector<Rule::Feature>& features, std::map<std::string, std::string>& game_values);
public:
    Instance(std::filesystem::path minecraft_path, std::string instance_name);
    Instance(Json::Value& json);
	const std::filesystem::path& minecraftPath();
	const std::string& instanceName();
	const std::string& id();
	const std::string& type();
	std::vector<std::string> generateLaunchCommand(InstanceContext& context, AccountsManager& account_manager, JavaManager& java_manager, const std::vector<Rule::Feature> features);
	std::string detectFabricVersion();
};