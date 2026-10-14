#pragma once

#include <string>

enum class TargetLanguage {
    Vietnamese,
    Japanese,
};

void loadDictionary();
std::string translateText(const std::string& english, TargetLanguage language);
