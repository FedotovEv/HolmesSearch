#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <map>
#include <functional>
#include <stdexcept>
#include <iterator>
#include <execution>
#include "document.h"
#include "paginator.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

enum class QueryError
{
    NO_QUERY_ERROR = 0,
    NO_TEXT_AFTER_MINUS,
    DOUBLE_MINUS,
    CONTAINS_SPECIAL_SYMBOLS
};

using FilterPred = std::function<bool(int, DocumentStatus, int)>;

class SearchServer
{
private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
        std::map<std::string_view, double> word_freqs; //Список слов документа и их обратных частот
    };

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query
    {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

public:

    template <template <typename ValueType> typename Container>
    explicit SearchServer(const Container<std::string>& text)
    {
        using std::operator""s;
        for (const std::string& word : text)
        {
            for (const unsigned char c : word)
                if (c < SPECIAL_SYMBOLS_MARGIN) throw std::invalid_argument("Стоп-слова : стоп-слово содержит недопустимые символы"s);
            if (!word.empty()) stop_words_.insert(word);
        }
    }

    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
                                   const std::vector<int>& ratings);

    std::vector<Document> FindTopDocuments(const std::string_view raw_query,
                                                DocumentStatus demand_status = DocumentStatus::ACTUAL) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query,
                                           FilterPred filter_pred) const;

    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query,
                                           DocumentStatus demand_status = DocumentStatus::ACTUAL) const
    {
        return FindTopDocuments(policy, raw_query,
                                [demand_status](int document_id, DocumentStatus status, int rating) -> bool
                                {return status == demand_status;});
    }

    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query,
                                           FilterPred filter_pred) const
    {
        using namespace std;

        QueryError query_error = QueryError::NO_QUERY_ERROR;

        const Query query = ParseQuery(raw_query, query_error);
        TestQueryErrorCode(query_error);

        auto matched_documents = FindAllDocuments(policy, query, filter_pred);

        sort(policy, matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs)
            {
                if (abs(lhs.relevance - rhs.relevance) < RELEVANCE_TOLERANCE)
                    return lhs.rating > rhs.rating;
                else
                    return lhs.relevance > rhs.relevance;
            });
        if (static_cast<int>(matched_documents.size()) > max_result_document_count)
            matched_documents.resize(max_result_document_count);

        return matched_documents;
    }

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query,
                                                                  int document_id) const;

    template <class _ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(_ExecutionPolicy&& policy,
                                                                            const std::string_view raw_query, int document_id) const
    {
        using namespace std;

        QueryError query_error = QueryError::NO_QUERY_ERROR;

        const Query query = ParseQuery(raw_query, query_error);
        TestQueryErrorCode(query_error);

        if (!documents_.count(document_id))
            throw out_of_range("Матчинг документов : неверный идентификатор документа"s);

        vector<string_view> filtered_plus_words(query.plus_words.begin(), query.plus_words.end());
        for_each(policy, filtered_plus_words.begin(), filtered_plus_words.end(),
                [this, document_id](string_view& current_plus_word)
                {
                    if (!word_to_document_freqs_.count(current_plus_word))
                        current_plus_word.remove_suffix(current_plus_word.size());
                    else
                        if (!word_to_document_freqs_.at(current_plus_word).count(document_id))
                            current_plus_word.remove_suffix(current_plus_word.size());
                });

        bool is_minus_word = false;
        for_each(policy, query.minus_words.begin(), query.minus_words.end(),
                        [this, document_id, &is_minus_word](const string_view& current_minus_word)
                        {
                            if (word_to_document_freqs_.count(current_minus_word))
                                if (word_to_document_freqs_.at(current_minus_word).count(document_id))
                                    is_minus_word = true;
                        });

        vector<string_view> result;
        if (!is_minus_word)
            for (const string_view& current_plus_word : filtered_plus_words)
                if (current_plus_word.size() > 0)
                    result.push_back(current_plus_word);

        return {result, documents_.at(document_id).status};
    }

    int GetDocumentCount() const;
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(int document_id);

    template <class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id)
    {
        if (!documents_.erase(document_id))
            return;
        std::for_each(policy, word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
                      [document_id](auto& word_to_document_pair)
                      {
                            word_to_document_pair.second.erase(document_id);
                      });

        // Зачищаем элементы словаря word_to_document_freqs_, для которых не осталось документов
        for (auto word_to_document_it = word_to_document_freqs_.begin();
            word_to_document_it != word_to_document_freqs_.end();)
        {
            auto next_it = next(word_to_document_it);
            if (!word_to_document_it->second.size())
                word_to_document_freqs_.erase(word_to_document_it);
            word_to_document_it = next_it;
        }
    }

    int GetSetResultDocumentCount(int new_result_document_count) const;

    class iterator : public std::iterator <std::bidirectional_iterator_tag, const int>
    {
    public:
        friend class SearchServer;

        iterator(const SearchServer *searchserver_ptr, bool begin_or_end);
        iterator& operator++();
        iterator operator++(int);
        iterator& operator--();
        iterator operator--(int);
        bool operator==(const iterator& second_op_it) const;
        bool operator!=(const iterator& second_op_it) const;
        const int& operator*() const;

    private:
        const SearchServer *linked_search_server_ptr;
        std::map<int, DocumentData>::const_iterator documents_it;
    };

    friend class iterator;

    int size() const
    {
        return GetDocumentCount();
    }
    iterator begin() const;
    iterator end() const;

private:

    // Некоторые константы, используемые в работе поисковиком
    static constexpr int DEFAULT_MAX_RESULT_DOCUMENT_COUNT = 5; // Умолчательное количество выдаваемых по запросу документов
    static constexpr double RELEVANCE_TOLERANCE = 1e-6;
    // Действительное, текущее количество выдаваемых по запросу документов
    mutable int max_result_document_count = DEFAULT_MAX_RESULT_DOCUMENT_COUNT;
    //Множество стоп-слов класса
    std::set<std::string, std::less<>> stop_words_;
    //Множество всех слов, имеющихся в зарегистрированных документах.
    std::set<std::string, std::less<>> words_collection_;
    // Словарь word_to_document_freqs_ преобразует слова запроса в список содержащих их документов.
    // Этот список, в свою очередь, содержит индекс документа и относительную частоту данного слова в документе.
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    //Словарь documents_ - список зарегистрированных в системе документов. Индекс эемента словаря - индекс документа,
    //содержание элемента словаря типа DocumentData - некоторая информация о нём.
    std::map<int, DocumentData> documents_;
    static const std::map<std::string_view, double> empty_word_freqs;

    //---- Частные функции класса SearchServer ------
    static void TestQueryErrorCode(QueryError& query_error);
    bool IsStopWord(std::string_view word) const;
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text,
                                                  bool *is_special_symbols = nullptr) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);
    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    QueryWord ParseQueryWord(std::string_view text, QueryError& query_word_error) const;
    Query ParseQuery(std::string_view text, QueryError& query_error) const;

    template <class ExecutionPolicy>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query, FilterPred document_predicate) const
    {
        using namespace std;

        ConcurrentMap<int, double> document_to_relevance(4096);

        auto plus_func = [this, &document_predicate, &document_to_relevance](const string_view word)
        {
            if (word_to_document_freqs_.count(word) == 0)
                return;
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                if (documents_.count(document_id))
                {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating))
                        document_to_relevance[document_id].refv += term_freq * inverse_document_freq;
                }
            }
        };

        for_each(policy, query.plus_words.begin(), query.plus_words.end(), plus_func);

        auto minus_func = [this, &document_predicate, &document_to_relevance](const string_view word)
        {
            if (word_to_document_freqs_.count(word) == 0)
                return;
            for (const auto [document_id, _] : word_to_document_freqs_.at(word))
                document_to_relevance.erase(document_id);
        };

        for_each(policy, query.minus_words.begin(), query.minus_words.end(), minus_func);

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap())
            matched_documents.push_back({document_id, relevance,
                                        documents_.at(document_id).rating});
        return matched_documents;
    }
};
