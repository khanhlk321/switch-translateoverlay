#pragma once

#include "translate.hpp"

#include <string>

bool networkStart();
void networkStop();
bool translateOnline(const std::string& english, TargetLanguage language, std::string& translated);
