#include <string>
#include <string_view>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <execution>
#include "search_server.h"

using namespace std;

const map<string_view, double> SearchServer::empty_word_freqs;

SearchServer::SearchServer(const string_view stop_words_text)
    : SearchServer(SplitIntoWordsString(stop_words_text))  // Делегирующий конструктор
{}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status,
                               const vector<int>& ratings)
{
    if (document_id < 0)
        throw invalid_argument("Добавление документа : индекс документа вне пределов допустимого диапазона"s);
    if (documents_.count(document_id))
        throw invalid_argument("Добавление документа : документ с данным индексом уже добавлен ранее"s);
    bool is_special_symbols;
    const vector<string_view> words = SplitIntoWordsNoStop(document, &is_special_symbols);
    if (is_special_symbols)
       	throw invalid_argument("Добавление документа : документ содержит недопустимые символы"s);

    const double inv_word_count = 1.0 / words.size();
    map<string_view, double> word_freqs;
    for (const string_view& word : words)
    {
        auto [wrd_col_it, is_added] = words_collection_.insert(static_cast<string>(word));
        word_to_document_freqs_[*wrd_col_it][document_id] += inv_word_count;
        word_freqs[*wrd_col_it] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, word_freqs});
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query,
                                  DocumentStatus demand_status) const
{
    return FindTopDocuments(execution::seq, raw_query,
                            [demand_status](int document_id, DocumentStatus status, int rating) -> bool
                            {return status == demand_status;});
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query,
                                                FilterPred filter_pred) const
{
    return FindTopDocuments(execution::seq, raw_query, filter_pred);
}

int SearchServer::GetDocumentCount() const
{
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const
{
    return SearchServer::MatchDocument(execution::seq, raw_query, document_id);
}

void SearchServer::TestQueryErrorCode(QueryError& query_error)
{
    if (query_error != QueryError::NO_QUERY_ERROR)
        switch (query_error)
        {
            case QueryError::NO_TEXT_AFTER_MINUS:
                throw invalid_argument("Неверный запрос : отсутствие текста после символа -"s);
            case QueryError::DOUBLE_MINUS:
                throw invalid_argument("Неверный запрос : двойной минус перед минус-словом"s);
            case QueryError::CONTAINS_SPECIAL_SYMBOLS:
                throw invalid_argument("Неверный запрос : запрос содержит недопустимые символы"s);
            default:
                throw invalid_argument("Неверный запрос : неизвестная ошибка"s);
        }
}

bool SearchServer::IsStopWord(string_view word) const
{
    return stop_words_.count(word) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view& text, bool *is_special_symbols) const
{
    vector<string_view> words;

    for (const string_view& word : SplitIntoWords(text, is_special_symbols))
        if (!IsStopWord(word)) words.push_back(word);
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings)
{
    if (ratings.empty()) return 0;
    return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text, QueryError& query_word_error) const
{
    bool is_minus = false;
    query_word_error = QueryError::NO_QUERY_ERROR;

    // Word shouldn't be empty
    if (text[0] == '-')
    {
        is_minus = true;
        if (text.size() < 2)
        {
            query_word_error = QueryError::NO_TEXT_AFTER_MINUS;
        }
        else
        {
            if (text[1] == '-') query_word_error = QueryError::DOUBLE_MINUS;
            text = text.substr(1);
        }
    }
    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text, QueryError& query_error) const
{
    Query query;
    bool is_special_symbols = false;
    query_error = QueryError::NO_QUERY_ERROR;

    auto words = SplitIntoWords(text, &is_special_symbols);
    if (is_special_symbols)
    {
        query_error = QueryError::CONTAINS_SPECIAL_SYMBOLS;
        return query;
    }
    for (const string_view& word : words)
    {
        QueryError query_word_error = QueryError::NO_QUERY_ERROR;
        const QueryWord query_word = ParseQueryWord(word, query_word_error);
        if (query_word_error != QueryError::NO_QUERY_ERROR)
        {
            query_error = query_word_error;
            return query;
        }
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
                query.minus_words.insert(query_word.data);
            else
                query.plus_words.insert(query_word.data);
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const
{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

SearchServer::iterator::iterator(const SearchServer *searchserver_ptr, bool begin_or_end) :
                                linked_search_server_ptr(searchserver_ptr)
{
    documents_it = begin_or_end ? linked_search_server_ptr->documents_.begin()
                                : linked_search_server_ptr->documents_.end();
}

SearchServer::iterator& SearchServer::iterator::operator++()
{
    ++documents_it;
    return *this;
}

SearchServer::iterator SearchServer::iterator::operator++(int)
{
    SearchServer::iterator temp = *this;
    ++(*this); //Собственно, инкремент
    return temp;
}

SearchServer::iterator& SearchServer::iterator::operator--()
{
    --documents_it;
    return *this;
}

SearchServer::iterator SearchServer::iterator::operator--(int)
{
    SearchServer::iterator temp = *this;
    --(*this); //Собственно, декремент
    return temp;
}

bool SearchServer::iterator::operator==(const SearchServer::iterator& second_op_it) const
{
    return linked_search_server_ptr == second_op_it.linked_search_server_ptr &&
           documents_it == second_op_it.documents_it;
}

bool SearchServer::iterator::operator!=(const SearchServer::iterator& second_op_it) const
{
    return !(*this == second_op_it);
}

const int& SearchServer::iterator::operator*() const
{
    return documents_it->first;
}

SearchServer::iterator SearchServer::begin() const
{
    return SearchServer::iterator(this, true);
}

SearchServer::iterator SearchServer::end() const
{
    return SearchServer::iterator(this, false);
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    if (documents_.count(document_id))
        return documents_.at(document_id).word_freqs;
    else
        return empty_word_freqs;
}

void SearchServer::RemoveDocument(int document_id)
{
    SearchServer::RemoveDocument(execution::seq, document_id);
}

int SearchServer::GetSetResultDocumentCount(int new_result_document_count) const
{
    int old_result_document_count = max_result_document_count;
    if (new_result_document_count >= 1)
        max_result_document_count = new_result_document_count;

    return old_result_document_count;

}
