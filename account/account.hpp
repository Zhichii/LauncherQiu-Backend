#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "../json_compat.hpp"

namespace Account {

struct OAuthConfig {
    std::string client_id;
    std::string redirect_uri = "http://localhost:23456/auth/callback";
    std::string scope = "XboxLive.signin offline_access";
    std::filesystem::path success_page = "account/pages/success.html";
    std::filesystem::path error_page = "account/pages/error.html";
};

struct MicrosoftAccount {
    std::string id;
    std::string account_type = "microsoft";
    std::string username;
    std::string uuid;
    std::string access_token;
    std::optional<std::string> refresh_token;
    std::optional<std::string> skin;
    std::int64_t created_at = 0;

    Json::Value toJson() const;
};

struct LoginSession {
    std::string auth_url;
    std::string state;
};

struct OAuthCallback {
    bool success = false;
    std::string code;
    std::string error;
    std::string state;
    std::map<std::string, std::string> query;
};

LoginSession createMicrosoftLoginSession(const OAuthConfig& config = {});
bool openBrowser(const std::string& url);
OAuthCallback parseCallbackUrl(const std::string& callback_url,
                               const std::string& expected_state = "");
OAuthCallback waitForOAuthCallback(
    const OAuthConfig& config = {},
    const std::string& expected_state = "",
    std::chrono::seconds timeout = std::chrono::seconds(300));
MicrosoftAccount completeMicrosoftAuth(const std::string& code,
                                       const OAuthConfig& config = {});
MicrosoftAccount completeMicrosoftAuthFromCallbackUrl(
    const std::string& callback_url,
    const OAuthConfig& config = {},
    const std::string& expected_state = "");
MicrosoftAccount loginMicrosoft(
    const OAuthConfig& config = {},
    std::chrono::seconds timeout = std::chrono::seconds(300));
MicrosoftAccount refreshMicrosoftAccount(const std::string& refresh_token,
                                         const OAuthConfig& config = {});

} // namespace Account
