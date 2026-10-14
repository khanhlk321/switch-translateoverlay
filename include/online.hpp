#pragma once

#include "translate.hpp"

#include <string>

bool networkStart();
void networkStop();
bool translateOnline(const std::string& text, SourceLanguage source, std::string& translated);
