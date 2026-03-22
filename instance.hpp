#pragma once
#include <filesystem>
#include "json_compat.hpp"

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
        bool isAllow(std::vector<Feature> features) const;
    };
	class LibraryItem {
		std::string _name;
		File _artifact;
		std::map<std::string,File> _classifiers;
		std::map<std::string,std::string> _natives;
		std::vector<Rule> _rules;
	public:
		LibraryItem(Json::Value json);
		bool extractNatives(std::filesystem::path game_dir, std::string instance_name);
		std::string name(); // 和_name不一样的是，这个会去除版本，只保留真正的名字。
		std::filesystem::path finalLibPath(std::filesystem::path game_dir) {
			std::filesystem::path path = game_dir;
			if (this->_artifact.path() != "") {
				path = path / "libraries" / this->_artifact.path();
			}
			else if (this->_natives.size() > 0) {
				libPath = Strings::strFormat("libraries\\%s", this->cn[this->nn[SYS_NAME]].path().c_str());
				for (char& i : libPath) if(i == O_PATHSEP[0]) i = PATHSEP[0];
			}
			else{
				std::vector<std::string> libNameSplit = Strings::split(this->n,":");
				libPath = this->n;
				bool f = 1;
				for (char& i : libPath) {
					if(f && i=='.') i = PATHSEP[0];
					if(i==':') {
						f = 0;
						i = PATHSEP[0];
					}
				}
				libPath = Strings::strFormat("libraries\\%s\\%s-%s.jar",
					libPath.c_str(), libNameSplit[1].c_str(), libNameSplit[2].c_str());
				for (char& i : libPath) if(i == O_PATHSEP[0]) i = PATHSEP[0];
			}
			return libPath;
		}
		bool allow(std::vector<Rule::Feature> features) {
			for (Rule& i : r) {
				if(!i.isAllow(features)) {
					return false;
				}
			}
			return true;
		}
	};
	class ArgumentItem {
		std::vector<std::string> v;
		std::vector<Rule> r;
	public:
		ArgumentItem(Json::Value json) {
			if(json.type() == Json::stringValue) {
				this->v = { json.asString() };
			}
			else {
				for (const auto& i : json["value"]) {
					this->v.push_back(i.asString());
				}
				if(json.isMember("rules")) {
					for (const auto& i : json["rules"]) {
						this->r.push_back(i);
					}
				}
			}
		}
		[[nodiscard]] bool allow(std::vector<Rule::Feature> features) {
			for (Rule& i : this->r) {
				if(!i.isAllow(features)) {
					return false;
				}
			}
			return true;
		}
		std::vector<std::string> value() { return this->v; }
	};
public:
    Instance(std::filesystem::path minecraft_path, std::string version);
};
