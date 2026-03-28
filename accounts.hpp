#pragma once

#include <string>

class Account {

};

 // Currently, hard-coded.
class AccountsManager {
public:
    void relogin();
    std::string userToken();
    std::string userName();
    std::string userId();
    bool online();
};