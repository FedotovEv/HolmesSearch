#pragma once
#include <string>
#include <vector>
#include <deque>
#include "document.h"
#include "search_server.h"

enum class RequestType
{
    UNKNOWN_REQUEST = 0,
    PREDICATE_REQUEST,
    STATUS_REQUEST
};

class RequestQueue
{
public:
    explicit RequestQueue(const SearchServer& search_server);
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для статистики
    std::vector<Document> AddFindRequest(const std::string& raw_query,
                                         DocumentStatus demand_status = DocumentStatus::ACTUAL);
    std::vector<Document> AddFindRequest(const std::string& raw_query, FilterPred filter_pred);

    void AddRequestToQueue(RequestType request_type, const std::string& raw_query,
                           std::vector<Document>& request_result);
    int GetNoResultRequests() const;

private:
    struct QueryResult
    {
        RequestType request_type;
        size_t result_counter;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& linked_search_server;
};
