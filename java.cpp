#include "java.hpp"

std::string JavaManager::find_java(int java_version) {
    if (java_version <= 16) return "/usr/lib/jvm/java-8-openjdk-amd64/bin/java";
    if (17 <= java_version && java_version <= 24) return "/usr/lib/jvm/java-21-openjdk-amd64/bin/java";
    if (25 <= java_version) return "/usr/bin/java";
}