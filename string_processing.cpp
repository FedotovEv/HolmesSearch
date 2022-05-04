#include <string>
#include <string_view>
#include <vector>
#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(const string_view& text, bool *is_special_symbols)
{
    vector<string_view> words;
    size_t start_word_position = 0;

    if (is_special_symbols) *is_special_symbols = false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        if (c < SPECIAL_SYMBOLS_MARGIN && is_special_symbols)
            *is_special_symbols = true;
        if (c == ' ')
        {
            string_view word(text);
            word.remove_suffix(text.size() - i);
            word.remove_prefix(start_word_position);
            if (!word.empty()) words.push_back(word);
            start_word_position = i + 1;
        }
    }
    if (start_word_position < text.size())
    {
        string_view word(text);
        word.remove_prefix(start_word_position);
        words.push_back(word);
    }
    return words;
}

vector<string> SplitIntoWordsString(const string_view& text, bool *is_special_symbols)
{
    vector<string> words;
    string word;

    if (is_special_symbols) *is_special_symbols = false;
    for (const unsigned char c : text)
    {
        if (c < SPECIAL_SYMBOLS_MARGIN && is_special_symbols)
            *is_special_symbols = true;
        if (c == ' ')
        {
            if (!word.empty()) words.push_back(word);
            word = "";
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty()) words.push_back(word);

    return words;
}
