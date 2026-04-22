#pragma once

#include <string>

struct VersionInfo {
    std::string id;
    std::string release_time;
    enum Type {
        PRE_CLASSIC, CLASSIC, INDEV, INFDEV, ALPHA, BETA,
        RELEASE, SNAPSHOT, PRE_RELEASE, RELEASE_CANDIDATE
    };
    Type type;
};