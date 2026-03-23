#pragma once

#include <vector>
#include <string>
#include <map>
#include <sstream>

namespace Strings {
	
	// 查找所有位置 by DeepSeek
	inline std::vector<size_t> find_all(std::string_view str, std::string_view substr) {
		std::vector<size_t> positions;
        for (size_t pos = 0;
             pos != std::string_view::npos;
        	 pos = str.find(substr, pos + 1)) {
            positions.push_back(pos);
        }
		return positions;
	}

	// 计数 by DeepSeek
	inline size_t count(std::string_view str, std::string_view substr) {
        size_t cnt = 0;
        for (size_t pos = 0; 
             pos != std::string_view::npos; 
             pos = str.find(substr, pos + 1)) {
            cnt++;
        }
        return cnt;
	}

	// Slice the string as Python. 
	// Include head without tail. 
	inline std::string sliceN(const std::string& baseStr, size_t from = 0, size_t to = 0, long long step = 1) {
		std::string newStr;
		if (from == to) to = baseStr.size() - to;
		if (from < 0) from = baseStr.size() + from;
		if (to <= 0) to = baseStr.size() + to;
		if (step == 0) step = 1;
		if (from > to) step = -step;
		if (to > baseStr.size()) to = baseStr.size();
		for (size_t i = from; (from < to) ? (i < to) : (i > to); i += step) {
			newStr += baseStr[i];
		}
		return newStr;
	}

	// Slice the string as Python, but step is always 1. 
	// Include head without tail. 
	inline std::string slice1(const std::string& baseStr, long long from = 0, long long to = 0) {
		if (from == to) to = baseStr.size() - to;
		if (from < 0) from = baseStr.size() + from;
		if (to <= 0) to = baseStr.size() + to;
		if (from > to) return "";
		if (to > baseStr.size()) to = baseStr.size();
		std::string newStr(baseStr.data()+from, to-from);
		return newStr;
	}

	// 替换所有 by DeepSeek
	inline std::string replace_all(const std::string& str, const std::string& from, const std::string& to) {
		std::ostringstream oss;
		size_t pos = 0;
		size_t last_pos = 0;
		
		while ((pos = str.find(from, last_pos)) != std::string::npos) {
			oss << str.substr(last_pos, pos - last_pos) << to;
			last_pos = pos + from.length();
		}
		oss << str.substr(last_pos);
		
		return oss.str();
	}

	// 分割字符串 by DeepSeek
	inline std::vector<std::string> split(std::string_view str, std::string_view delim) {
        if (delim.empty()) return { std::string(str) };
        
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = 0;
        
        while ((end = str.find(delim, start)) != std::string_view::npos) {
            result.emplace_back(str.substr(start, end - start));
            start = end + delim.length();
        }
        result.emplace_back(str.substr(start));
        
        return result;
    }

	// 连接字符串
	inline std::string join(const std::vector<std::string>& vec, const std::string& joiner) {
		if (vec.size() == 0) return {};
		std::ostringstream oss;
		oss << vec[0];
		for (size_t i = 1; i < vec.size(); i++) {
			oss << joiner;
			oss << vec[i];
		}
		return oss.str();
	}

    // 取中间 by DeepSeek
    inline std::string_view between(std::string_view str, std::string_view start, std::string_view end) {
        size_t start_pos = start.empty() ? 0 : str.find(start);
        if (start_pos == std::string_view::npos) return {};
        
        size_t begin = start_pos + start.length();
        size_t end_pos = end.empty() ? str.size() : str.find(end, begin);
        if (end_pos == std::string_view::npos) return {};
        
        return str.substr(begin, end_pos - begin);
    }

	/* 懒得维护

	C++ 20有starts_with和ends_with了！！
	// Check if str starts with substr. 
	bool startsWith(const std::string& str, const std::string& substr) {
		if (str.size() < substr.size()) return false;
		if (str == substr) return true;
		for (size_t i = 0; i < substr.size(); i++) {
			if (substr[i] != str[i]) return false;
		}
		return true;
	}

	// Convert string to wide string. 
	std::wstring s2ws(const std::string& s) {
		const char* str = s.c_str();
		size_t lenO = s.size();
		size_t len = s.size() + 1;
		size_t lenO_ = lenO;
		size_t len_ = len;
		wchar_t* wstr = new wchar_t[len];
		memset(wstr, 0, len*2);
		#ifdef _WIN32
			errno_t i = MultiByteToWideChar(CP_UTF8, MB_USEGLYPHCHARS, str, (int)lenO, wstr, (int)len);
		#else
			mbstowcs(wstr, str, len);
		#endif
		std::wstring ret(wstr);
		if (i == EILSEQ) {
			writeLog("Failed to convert string to wstring. ");
			writeLog("LenO: %u. Len: %u. Errcode: %u", lenO_, len_, i);
		}
		delete[] wstr;
		return ret;
	}

	// Convert wide string to string. 
	std::string ws2s(const std::wstring& ws) {
		const wchar_t* wstr = ws.c_str();
		size_t lenO = ws.size();
		size_t len = 2 * ws.size() + 1;
		size_t lenO_ = lenO;
		size_t len_ = len;
		char* str = new char[len];
		memset(str, 0, len);
		#ifdef _WIN32
			errno_t i = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, wstr, (int)lenO, str, (int)len, NULL, NULL);// wcstombs_s(&lenO, str, len - 1, wstr, len);
		#else
			errno_t i = wcstombs(str, wstr, len);
		#endif
		std::string ret(str);
		if (i == EILSEQ) {
			writeLog("Failed to convert wstring to string. ");
			writeLog("LenO: %u. Len: %u. Errcode: %u", lenO_, len_, i);
		}
		delete[] str;
		return ret;
	}

	// Format the string, same with sprintf(). Note: use .c_str() in the arguments. 
	std::string strFormat(const std::string format, ...) {
		std::string dst;
		va_list args;
		va_start(args, format);
		char* temp = new char[16384];
		_vsprintf_s_l(temp, 16384, format.c_str(), NULL, args);
		dst = temp;
		delete[] temp;
		va_end(args);
		return dst;
	}
	std::wstring strFormat(const std::wstring format, ...) {
		std::wstring dst;
		va_list args;
		va_start(args, format);
		wchar_t* temp = new wchar_t[16384];
		_vswprintf_s_l(temp, 16384, format.c_str(), NULL, args);
		dst = temp;
		delete[] temp;
		va_end(args);
		return dst;
	}
		
	暂时懒得维护。请看std::filesystem::path
	// Format the string of a directory. 
	std::string formatDirStr(const std::string& path) {
		std::string o = path;
		for (size_t i = 0; i < o.size(); i++) {
			if (o[i] == O_PATHSEP[0]) {
				o[i] = PATHSEP[0];
			}
		}
		while (count(o, DPATHSEP)) {
			o = replace(o, DPATHSEP, PATHSEP);
		}
		std::vector<size_t> pos = find(o, ":");
		if (pos.size() != 0) {
			size_t p = pos[0];
			o[p] = '?';
			o = replace(o, ":", "");
			o[p] = ':';
			if (o.size() > p+1) {
				if (o[p+1] != PATHSEP[0])
					o = replace(o, ":", std::string(":") + PATHSEP);
			}
		}
		if (startsWith(path, "\\\\")) {
			o = "\\" + o;
		}
		if (o[o.size()-1] != PATHSEP[0]) {
			o += PATHSEP;
		}
		return o;
	}
	*/

}