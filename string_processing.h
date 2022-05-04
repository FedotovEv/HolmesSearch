#pragma once
#include <string>
#include <string_view>
#include <vector>

static constexpr char SPECIAL_SYMBOLS_MARGIN = 32;

std::vector<std::string_view> SplitIntoWords(const std::string_view& text,
                                             bool *is_special_symbols = nullptr);
std::vector<std::string> SplitIntoWordsString(const std::string_view& text,
                                   bool *is_special_symbols = nullptr);
