#pragma once

#include <string>

enum class SourceLanguage {
    English,
    Japanese,
};

void loadDictionary();
std::string translateText(const std::string& text, SourceLanguage source);
