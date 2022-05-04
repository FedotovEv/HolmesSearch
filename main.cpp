
#include "process_queries.h"
#include "search_server.h"
#include "log_duration.h"

#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std;

#define TEST(processor) Test(#processor, processor, test_search_server, test_queries)

string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int max_word_count) {
    const int word_count = uniform_int_distribution(1, max_word_count)(generator);
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename QueriesProcessor>
void Test(string_view mark, QueriesProcessor processor, const SearchServer& search_server, const vector<string>& queries) {
    LOG_DURATION(mark);
    const auto documents = processor(search_server, queries);
    cout << documents.size() << endl;
}

template <typename ExecutionPolicy>
void Test2(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy)
{
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const string_view query : queries)
        for (const auto& document : search_server.FindTopDocuments(policy, query))
            total_relevance += document.relevance;
    cout << total_relevance << endl;
}

#define TEST2(mode, policy) Test2(#mode, search_server, queries, execution::policy)

void PrintDocument(const Document& document)
{
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main()
{
    const vector<string> docs =
    {
        "funny pet and nasty rat"s,
        "funny pet with curly hair"s,
        "funny pet and not very nasty rat"s,
        "pet with rat and rat and rat"s,
        "nasty rat with curly hair"s,
    };

    const vector<string> queries =
    {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };

    {
        SearchServer search_server("and with"s);

        int id = 0;
        for (const string& text : docs)
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});

        id = 0;
        for (const auto& documents : ProcessQueries(search_server, queries))
        {
            cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
        }

        for (const Document& document : ProcessQueriesJoined(search_server, queries))
            cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
    }

    {
        /*mt19937 generator;
        const auto dictionary = GenerateDictionary(generator, 10000, 25);
        const auto documents = GenerateQueries(generator, dictionary, 100'000, 10);

        SearchServer test_search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i)
            test_search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});

        const auto test_queries = GenerateQueries(generator, dictionary, 10'000, 7);
        cout << "Run" << endl;
        TEST(ProcessQueriesJoined);*/
    }

    {
        SearchServer search_server("and with"s);

        int id = 0;
        for (const string& text : docs)
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});

        const string query = "curly and funny"s;

        auto report = [&search_server, &query]
        {
            cout << search_server.GetDocumentCount() << " documents total, "s
                << search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << endl;
        };

        report();
        // однопоточная версия
        search_server.RemoveDocument(5);
        report();
        // однопоточная версия
        search_server.RemoveDocument(execution::seq, 1);
        report();
        // многопоточная версия
        search_server.RemoveDocument(execution::par, 2);
        report();
    }

    {
        SearchServer search_server("and with"s);

        int id = 0;
        for (const string& text : docs)
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});

        const string query = "curly and funny -not"s;
        const string_view query_view(query);

        {
            const auto [words, status] = search_server.MatchDocument(query, 1);
            cout << words.size() << " words for document 1"s << endl;
            // 1 words for document 1
        }

        {
            const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
            cout << words.size() << " words for document 2"s << endl;
            // 2 words for document 2
        }

        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
            cout << words.size() << " words for document 3"s << endl;
            // 0 words for document 3
        }

        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query_view, 2);
            cout << words.size() << " words for document 2"s << endl;
            cout << words[0] << " .. " << words[1] << endl;
            // 2 words for document 2
        }
    }

    {
        vector<string_view> docs =
        {
            "funny pet and nasty rat",
            "funny pet with curly hair",
            "funny pet and not very nasty rat",
            "pet with rat and rat and rat",
            "nasty rat with curly hair",
        };

        SearchServer search_server("and with"s);

        int id = 0;
        for (const string_view& text : docs)
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, id});

        for (string_view& text : docs)
            text = ""sv;

        const string query = "curly and funny"s;
        const string_view query_view = "and hair";
        cout << "-----" << endl;
        for (auto doc : search_server.FindTopDocuments(query))
            cout << doc.id << ' ' << doc.relevance << ' ' << doc.rating << endl;
        cout << "-----" << endl;
        for (auto doc : search_server.FindTopDocuments(query_view))
            cout << doc.id << ' ' << doc.relevance << ' ' << doc.rating << endl;
        cout << "-----" << endl;
        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query, 2);
            cout << words.size() << " words for document 2"s << endl;
            cout << words[0] << " .. " << words[1] << endl;
        }
        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query_view, 2);
            cout << words.size() << " words for document 2"s << endl;
            cout << words[0] << endl;
        }
    }

    {
        SearchServer search_server("and with"s);

        int id = 0;
        for (
            const string& text :
            {
                "white cat and yellow hat"s,
                "curly cat curly tail"s,
                "nasty dog with big eyes"s,
                "nasty pigeon john"s,
            }
        )
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});

        cout << "ACTUAL by default:"s << endl;
            // последовательная версия
        for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s))
            PrintDocument(document);

        cout << "BANNED:"s << endl;
            // последовательная версия
        for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED))
            PrintDocument(document);

        cout << "Even ids:"s << endl;
            // параллельная версия
        for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; }))
            PrintDocument(document);
    }

    {
        mt19937 generator;

        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i)
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});

        const auto queries = GenerateQueries(generator, dictionary, 100, 70);

        TEST2("Sequential", seq);
        TEST2("Parallel", par);
    }

    return 0;
}

