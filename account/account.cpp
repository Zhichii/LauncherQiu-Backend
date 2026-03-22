#include "account.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Account {
namespace {

struct ParsedHttpUrl {
    std::string host;
    unsigned short port = 80;
    std::string path = "/";
};

struct HttpResponse {
    long status_code = 0;
    std::string body;
};

struct CommandResult {
    int exit_code = 0;
    std::string output;
};

class TempFiles {
    std::vector<std::filesystem::path> files_;

public:
    void track(const std::filesystem::path& path) {
        files_.push_back(path);
    }

    ~TempFiles() {
        for (const auto& path : files_) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
};

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

class SocketRuntime {
public:
    SocketRuntime() {
#if defined(_WIN32)
        WSADATA wsa_data;
        const int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (rc != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~SocketRuntime() {
#if defined(_WIN32)
        WSACleanup();
#endif
    }
};

std::string trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(),
                             [&](unsigned char ch) { return !is_space(ch); }));
    value.erase(
        std::find_if(value.rbegin(), value.rend(),
                     [&](unsigned char ch) { return !is_space(ch); })
            .base(),
        value.end());
    return value;
}

std::string readFileOrFallback(const std::filesystem::path& path,
                               std::string fallback) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return fallback;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::string jsonToString(const Json::Value& json) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json);
}

Json::Value parseJsonOrThrow(const std::string& text) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream iss(text);
    if (!Json::parseFromStream(builder, iss, &root, &errors)) {
        throw std::runtime_error("Failed to parse JSON: " + errors);
    }
    return root;
}

std::string hexByte(unsigned char value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out(2, '0');
    out[0] = kDigits[(value >> 4) & 0xF];
    out[1] = kDigits[value & 0xF];
    return out;
}

std::string urlEncode(std::string_view input) {
    std::string out;
    out.reserve(input.size() * 3);
    for (unsigned char ch : input) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' ||
            ch == '~') {
            out.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out += hexByte(ch);
        }
    }
    return out;
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

std::string urlDecode(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '+') {
            out.push_back(' ');
        } else if (ch == '%' && i + 2 < input.size()) {
            const int hi = hexValue(input[i + 1]);
            const int lo = hexValue(input[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(ch);
            }
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

std::string randomHexString(std::size_t bytes) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        out += hexByte(static_cast<unsigned char>(dist(rng)));
    }
    return out;
}

std::string makeTempToken() {
    const auto now =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::to_string(now) + "_" + randomHexString(8);
}

std::filesystem::path writeTempFile(const std::string& suffix,
                                    const std::string& content) {
    const auto path =
        std::filesystem::temp_directory_path() / ("launcherqiu_" + makeTempToken() + suffix);
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to create temp file: " + path.string());
    }
    ofs << content;
    return path;
}

CommandResult runCommand(const std::string& command) {
#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        throw std::runtime_error("Failed to run command: " + command);
    }

    std::array<char, 4096> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

#if defined(_WIN32)
    const int exit_code = _pclose(pipe);
#else
    const int exit_code = pclose(pipe);
#endif
    return { exit_code, output };
}

HttpResponse runCurlRequest(const std::string& method,
                            const std::string& url,
                            const std::vector<std::string>& headers,
                            const std::optional<std::string>& body = std::nullopt) {
    TempFiles temp_files;

    std::ostringstream cfg;
    cfg << "silent\n";
    cfg << "show-error\n";
    cfg << "location\n";
    cfg << "request = \"" << method << "\"\n";
    cfg << "url = \"" << url << "\"\n";
    for (const auto& header : headers) {
        cfg << "header = \"" << header << "\"\n";
    }

    if (body.has_value()) {
        const auto body_path = writeTempFile(".body", *body);
        temp_files.track(body_path);
        cfg << "data-binary = \"@" << body_path.generic_string() << "\"\n";
    }

    cfg << "write-out = \"\\n%{http_code}\"\n";

    const auto cfg_path = writeTempFile(".curl", cfg.str());
    temp_files.track(cfg_path);

    const std::string command = "curl --config \"" + cfg_path.string() + "\"";
    const CommandResult result = runCommand(command);
    if (result.exit_code != 0) {
        throw std::runtime_error("curl request failed: " + trim(result.output));
    }

    const std::size_t split = result.output.find_last_of('\n');
    if (split == std::string::npos) {
        throw std::runtime_error("curl response missing HTTP code");
    }

    HttpResponse response;
    response.body = result.output.substr(0, split);
    response.status_code = std::strtol(trim(result.output.substr(split + 1)).c_str(),
                                       nullptr, 10);
    return response;
}

ParsedHttpUrl parseHttpUrl(const std::string& url) {
    const std::string prefix = "http://";
    if (!url.starts_with(prefix)) {
        throw std::runtime_error("Only http redirect URIs are supported");
    }

    const std::string remainder = url.substr(prefix.size());
    const std::size_t slash = remainder.find('/');
    const std::string host_port =
        slash == std::string::npos ? remainder : remainder.substr(0, slash);
    const std::string path =
        slash == std::string::npos ? "/" : remainder.substr(slash);

    ParsedHttpUrl parsed;
    parsed.path = path.empty() ? "/" : path;

    const std::size_t colon = host_port.rfind(':');
    if (colon == std::string::npos) {
        parsed.host = host_port;
    } else {
        parsed.host = host_port.substr(0, colon);
        parsed.port = static_cast<unsigned short>(
            std::stoi(host_port.substr(colon + 1)));
    }

    if (parsed.host.empty()) {
        throw std::runtime_error("Redirect URI host is empty");
    }
    return parsed;
}

void closeSocket(SocketHandle socket_handle) {
    if (socket_handle == kInvalidSocket) {
        return;
    }
#if defined(_WIN32)
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

std::string httpResponseText(const std::string& body,
                             std::string_view status_text = "200 OK") {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_text << "\r\n";
    oss << "Content-Type: text/html; charset=utf-8\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

std::string recvHttpRequest(SocketHandle client_socket) {
    std::array<char, 8192> buffer{};
    std::string request;

    while (request.find("\r\n\r\n") == std::string::npos) {
        const int received =
#if defined(_WIN32)
            recv(client_socket, buffer.data(),
                 static_cast<int>(buffer.size()), 0);
#else
            recv(client_socket, buffer.data(), buffer.size(), 0);
#endif
        if (received <= 0) {
            break;
        }
        request.append(buffer.data(), received);
        if (request.size() > 65536) {
            break;
        }
    }

    return request;
}

std::string extractRequestTarget(const std::string& request) {
    const std::size_t line_end = request.find("\r\n");
    if (line_end == std::string::npos) {
        throw std::runtime_error("Malformed HTTP request");
    }
    const std::string line = request.substr(0, line_end);

    const std::size_t method_end = line.find(' ');
    const std::size_t target_end = line.find(' ', method_end + 1);
    if (method_end == std::string::npos || target_end == std::string::npos) {
        throw std::runtime_error("Malformed HTTP request line");
    }
    return line.substr(method_end + 1, target_end - method_end - 1);
}

std::map<std::string, std::string> parseQueryString(const std::string& query) {
    std::map<std::string, std::string> values;
    std::size_t begin = 0;
    while (begin <= query.size()) {
        const std::size_t end = query.find('&', begin);
        const std::string token = query.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        if (!token.empty()) {
            const std::size_t equal = token.find('=');
            const std::string key = token.substr(0, equal);
            const std::string value =
                equal == std::string::npos ? "" : token.substr(equal + 1);
            values[urlDecode(key)] = urlDecode(value);
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return values;
}

OAuthCallback parseCallbackTarget(const std::string& target,
                                  const std::string& expected_state) {
    OAuthCallback callback;
    const std::size_t question = target.find('?');
    const std::string query = question == std::string::npos ? "" : target.substr(question + 1);
    callback.query = parseQueryString(query);

    if (const auto it = callback.query.find("state"); it != callback.query.end()) {
        callback.state = it->second;
    }

    if (!expected_state.empty() && callback.state != expected_state) {
        callback.error = "OAuth state mismatch";
        return callback;
    }

    if (const auto it = callback.query.find("error"); it != callback.query.end()) {
        callback.error = callback.query.count("error_description")
                             ? callback.query["error_description"]
                             : it->second;
        return callback;
    }

    if (const auto it = callback.query.find("code"); it != callback.query.end() &&
                                           !it->second.empty()) {
        callback.success = true;
        callback.code = it->second;
        return callback;
    }

    callback.error = "No authorization code found";
    return callback;
}

Json::Value postFormJson(const std::string& url,
                         const std::map<std::string, std::string>& form,
                         long* status_code = nullptr) {
    std::vector<std::string> parts;
    parts.reserve(form.size());
    for (const auto& [key, value] : form) {
        parts.push_back(urlEncode(key) + "=" + urlEncode(value));
    }

    const HttpResponse response =
        runCurlRequest("POST", url,
                       {
                           "Content-Type: application/x-www-form-urlencoded",
                           "Accept: application/json",
                       },
                       std::accumulate(parts.begin(), parts.end(), std::string(),
                                       [](std::string lhs, const std::string& rhs) {
                                           if (!lhs.empty()) {
                                               lhs += "&";
                                           }
                                           lhs += rhs;
                                           return lhs;
                                       }));
    if (status_code != nullptr) {
        *status_code = response.status_code;
    }
    return parseJsonOrThrow(response.body);
}

Json::Value postJson(const std::string& url,
                     const Json::Value& body,
                     const std::vector<std::string>& extra_headers = {},
                     long* status_code = nullptr) {
    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Accept: application/json",
    };
    headers.insert(headers.end(), extra_headers.begin(), extra_headers.end());

    const HttpResponse response =
        runCurlRequest("POST", url, headers, jsonToString(body));
    if (status_code != nullptr) {
        *status_code = response.status_code;
    }
    return parseJsonOrThrow(response.body);
}

Json::Value getJson(const std::string& url,
                    const std::vector<std::string>& headers = {},
                    long* status_code = nullptr) {
    const HttpResponse response = runCurlRequest("GET", url, headers);
    if (status_code != nullptr) {
        *status_code = response.status_code;
    }
    return parseJsonOrThrow(response.body);
}

std::string xstsErrorMessage(const Json::Value& response) {
    if (!response.isMember("XErr")) {
        return {};
    }

    const auto code = response["XErr"].asLargestUInt();
    switch (code) {
    case 2148916233ULL:
        return "This account does not have an Xbox profile yet";
    case 2148916235ULL:
        return "Xbox Live is not available in this region";
    case 2148916236ULL:
    case 2148916237ULL:
        return "This account requires adult verification";
    case 2148916238ULL:
        return "This minor account must be added to a family";
    default:
        return "Xbox Live authentication failed";
    }
}

struct MinecraftTokenData {
    std::string access_token;
};

struct MinecraftProfile {
    std::string id;
    std::string name;
    std::optional<std::string> skin_url;
};

MinecraftTokenData getMinecraftToken(const std::string& ms_access_token) {
    Json::Value xbl_body;
    xbl_body["Properties"]["AuthMethod"] = "RPS";
    xbl_body["Properties"]["SiteName"] = "user.auth.xboxlive.com";
    xbl_body["Properties"]["RpsTicket"] = "d=" + ms_access_token;
    xbl_body["RelyingParty"] = "http://auth.xboxlive.com";
    xbl_body["TokenType"] = "JWT";

    const Json::Value xbl_response = postJson(
        "https://user.auth.xboxlive.com/user/authenticate", xbl_body);

    if (!xbl_response.isMember("Token")) {
        throw std::runtime_error("Invalid Xbox Live token response");
    }
    const std::string xbl_token = xbl_response["Token"].asString();

    Json::Value xsts_body;
    xsts_body["Properties"]["SandboxId"] = "RETAIL";
    xsts_body["Properties"]["UserTokens"].append(xbl_token);
    xsts_body["RelyingParty"] = "rp://api.minecraftservices.com/";
    xsts_body["TokenType"] = "JWT";

    const Json::Value xsts_response =
        postJson("https://xsts.auth.xboxlive.com/xsts/authorize", xsts_body);

    if (const std::string xerr = xstsErrorMessage(xsts_response); !xerr.empty()) {
        throw std::runtime_error(xerr);
    }

    if (!xsts_response.isMember("Token") ||
        !xsts_response["DisplayClaims"].isMember("xui") ||
        !xsts_response["DisplayClaims"]["xui"].isArray() ||
        xsts_response["DisplayClaims"]["xui"].empty() ||
        !xsts_response["DisplayClaims"]["xui"][0].isMember("uhs")) {
        throw std::runtime_error("Invalid XSTS token response");
    }

    const std::string xsts_token = xsts_response["Token"].asString();
    const std::string uhs =
        xsts_response["DisplayClaims"]["xui"][0]["uhs"].asString();

    Json::Value minecraft_body;
    minecraft_body["identityToken"] = "XBL3.0 x=" + uhs + ";" + xsts_token;

    const Json::Value minecraft_response = postJson(
        "https://api.minecraftservices.com/authentication/login_with_xbox",
        minecraft_body);

    if (!minecraft_response.isMember("access_token")) {
        throw std::runtime_error("Invalid Minecraft token response");
    }

    return { minecraft_response["access_token"].asString() };
}

MinecraftProfile getMinecraftProfile(const std::string& minecraft_token) {
    long status_code = 0;
    const Json::Value profile = getJson(
        "https://api.minecraftservices.com/minecraft/profile",
        { "Authorization: Bearer " + minecraft_token }, &status_code);

    if (status_code >= 400 || profile.isMember("error") ||
        profile.isMember("errorMessage")) {
        throw std::runtime_error(
            "Failed to fetch Minecraft profile; the account may not own the game");
    }

    if (!profile.isMember("id") || !profile.isMember("name")) {
        throw std::runtime_error("Invalid Minecraft profile response");
    }

    MinecraftProfile result;
    result.id = profile["id"].asString();
    result.name = profile["name"].asString();

    if (profile.isMember("skins") && profile["skins"].isArray() &&
        !profile["skins"].empty() && profile["skins"][0].isMember("url")) {
        result.skin_url = profile["skins"][0]["url"].asString();
    }
    return result;
}

std::string defaultSuccessPage() {
    return "<html><body><h1>Login successful</h1><p>You can close this page now.</p></body></html>";
}

std::string defaultErrorPage() {
    return "<html><body><h1>Login failed</h1><p>Please return to the launcher and try again.</p></body></html>";
}

std::int64_t nowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

Json::Value MicrosoftAccount::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["type"] = account_type;
    json["account_type"] = account_type;
    json["username"] = username;
    json["uuid"] = uuid;
    json["accessToken"] = access_token;
    json["access_token"] = access_token;
    json["createdAt"] = static_cast<Json::Int64>(created_at);
    json["created_at"] = static_cast<Json::Int64>(created_at);
    json["refreshToken"] = refresh_token.has_value() ? *refresh_token : "";
    json["refresh_token"] = refresh_token.has_value() ? *refresh_token : "";
    if (skin.has_value()) {
        json["skin"] = *skin;
    } else {
        json["skin"] = Json::nullValue;
    }
    return json;
}

LoginSession createMicrosoftLoginSession(const OAuthConfig& config) {
    LoginSession session;
    session.state = randomHexString(16);
    session.auth_url =
        "https://login.live.com/oauth20_authorize.srf"
        "?client_id=" + urlEncode(config.client_id) +
        "&response_type=code"
        "&redirect_uri=" + urlEncode(config.redirect_uri) +
        "&scope=" + urlEncode(config.scope) +
        "&state=" + urlEncode(session.state) +
        "&prompt=select_account";
    return session;
}

bool openBrowser(const std::string& url) {
#if defined(_WIN32)
    const std::string command = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
    const std::string command = "open \"" + url + "\"";
#else
    const std::string command = "xdg-open \"" + url + "\"";
#endif
    return std::system(command.c_str()) == 0;
}

OAuthCallback parseCallbackUrl(const std::string& callback_url,
                               const std::string& expected_state) {
    const std::size_t question = callback_url.find('?');
    if (question == std::string::npos) {
        OAuthCallback callback;
        callback.error = "Invalid callback URL";
        return callback;
    }
    return parseCallbackTarget(callback_url.substr(question), expected_state);
}

OAuthCallback waitForOAuthCallback(const OAuthConfig& config,
                                   const std::string& expected_state,
                                   std::chrono::seconds timeout) {
    const ParsedHttpUrl redirect = parseHttpUrl(config.redirect_uri);
    if (redirect.host != "localhost" && redirect.host != "127.0.0.1") {
        throw std::runtime_error("Redirect URI host must be localhost or 127.0.0.1");
    }

    SocketRuntime runtime;
    SocketHandle server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == kInvalidSocket) {
        throw std::runtime_error("Failed to create callback server socket");
    }

    int reuse = 1;
#if defined(_WIN32)
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(redirect.port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closeSocket(server);
        throw std::runtime_error("Failed to bind callback server");
    }

    if (listen(server, 1) != 0) {
        closeSocket(server);
        throw std::runtime_error("Failed to listen on callback server");
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(server, &read_set);

    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count());
    tv.tv_usec = 0;

    const int select_result = select(
#if defined(_WIN32)
        0,
#else
        server + 1,
#endif
        &read_set, nullptr, nullptr, &tv);

    if (select_result <= 0) {
        closeSocket(server);
        throw std::runtime_error("Timed out while waiting for login callback");
    }

    SocketHandle client = accept(server, nullptr, nullptr);
    closeSocket(server);
    if (client == kInvalidSocket) {
        throw std::runtime_error("Failed to accept callback request");
    }

    const std::string request = recvHttpRequest(client);
    OAuthCallback callback;
    try {
        const std::string target = extractRequestTarget(request);
        const std::size_t question = target.find('?');
        const std::string path =
            question == std::string::npos ? target : target.substr(0, question);
        if (path != redirect.path) {
            callback.error = "Invalid callback path";
        } else {
            callback = parseCallbackTarget(target, expected_state);
        }
    } catch (const std::exception& ex) {
        callback.error = ex.what();
    }

    const std::string success_html =
        readFileOrFallback(config.success_page, defaultSuccessPage());
    const std::string error_html =
        readFileOrFallback(config.error_page, defaultErrorPage());
    const std::string response_text = httpResponseText(
        callback.success ? success_html : error_html,
        callback.success ? "200 OK" : "400 Bad Request");

    send(client, response_text.data(),
#if defined(_WIN32)
         static_cast<int>(response_text.size()),
#else
         response_text.size(),
#endif
         0);
    closeSocket(client);

    return callback;
}

MicrosoftAccount completeMicrosoftAuth(const std::string& code,
                                       const OAuthConfig& config) {
    long status_code = 0;
    const Json::Value microsoft_token = postFormJson(
        "https://login.live.com/oauth20_token.srf",
        {
            { "client_id", config.client_id },
            { "code", code },
            { "grant_type", "authorization_code" },
            { "redirect_uri", config.redirect_uri },
        },
        &status_code);

    if (status_code >= 400 || microsoft_token.isMember("error")) {
        const std::string error =
            microsoft_token.isMember("error_description")
                ? microsoft_token["error_description"].asString()
                : "Failed to get Microsoft token";
        throw std::runtime_error(error);
    }

    if (!microsoft_token.isMember("access_token")) {
        throw std::runtime_error("Microsoft access_token missing");
    }

    const std::string ms_access_token = microsoft_token["access_token"].asString();
    std::optional<std::string> refresh_token;
    if (microsoft_token.isMember("refresh_token")) {
        refresh_token = microsoft_token["refresh_token"].asString();
    }

    const MinecraftTokenData mc_token = getMinecraftToken(ms_access_token);
    const MinecraftProfile profile = getMinecraftProfile(mc_token.access_token);

    MicrosoftAccount account;
    account.id = profile.id;
    account.username = profile.name;
    account.uuid = profile.id;
    account.access_token = mc_token.access_token;
    account.refresh_token = refresh_token;
    account.skin = profile.skin_url;
    account.created_at = nowMillis();
    return account;
}

MicrosoftAccount completeMicrosoftAuthFromCallbackUrl(
    const std::string& callback_url,
    const OAuthConfig& config,
    const std::string& expected_state) {
    const OAuthCallback callback = parseCallbackUrl(callback_url, expected_state);
    if (!callback.success) {
        throw std::runtime_error(callback.error.empty() ? "Login failed" : callback.error);
    }
    return completeMicrosoftAuth(callback.code, config);
}

MicrosoftAccount loginMicrosoft(const OAuthConfig& config,
                                std::chrono::seconds timeout) {
    const LoginSession session = createMicrosoftLoginSession(config);
    if (!openBrowser(session.auth_url)) {
        throw std::runtime_error("Failed to open browser");
    }

    const OAuthCallback callback =
        waitForOAuthCallback(config, session.state, timeout);
    if (!callback.success) {
        throw std::runtime_error(callback.error.empty() ? "Login failed" : callback.error);
    }
    return completeMicrosoftAuth(callback.code, config);
}

MicrosoftAccount refreshMicrosoftAccount(const std::string& refresh_token,
                                         const OAuthConfig& config) {
    long status_code = 0;
    const Json::Value microsoft_token = postFormJson(
        "https://login.live.com/oauth20_token.srf",
        {
            { "client_id", config.client_id },
            { "refresh_token", refresh_token },
            { "grant_type", "refresh_token" },
            { "scope", config.scope },
        },
        &status_code);

    if (status_code >= 400 || microsoft_token.isMember("error")) {
        throw std::runtime_error("Failed to refresh Microsoft token");
    }

    if (!microsoft_token.isMember("access_token")) {
        throw std::runtime_error("Microsoft access_token missing");
    }

    const std::string ms_access_token = microsoft_token["access_token"].asString();
    std::optional<std::string> new_refresh_token = refresh_token;
    if (microsoft_token.isMember("refresh_token")) {
        new_refresh_token = microsoft_token["refresh_token"].asString();
    }

    const MinecraftTokenData mc_token = getMinecraftToken(ms_access_token);
    const MinecraftProfile profile = getMinecraftProfile(mc_token.access_token);

    MicrosoftAccount account;
    account.id = profile.id;
    account.username = profile.name;
    account.uuid = profile.id;
    account.access_token = mc_token.access_token;
    account.refresh_token = new_refresh_token;
    account.skin = profile.skin_url;
    account.created_at = nowMillis();
    return account;
}

} // namespace Account
