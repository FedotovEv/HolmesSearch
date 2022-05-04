#include <string>
#include <vector>
#include "document.h"
#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server) : linked_search_server(search_server)
{}

// "переходники" для всех методов поиска, чтобы сохранять результаты для статистики

vector<Document> RequestQueue::AddFindRequest(const string& raw_query,
                                              DocumentStatus demand_status)
{
    vector<Document> request_resull;
    request_resull = linked_search_server.FindTopDocuments(raw_query, demand_status);
    AddRequestToQueue(RequestType::STATUS_REQUEST, raw_query, request_resull);
    return request_resull;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, FilterPred filter_pred)
{
    vector<Document> request_resull;
    request_resull = linked_search_server.FindTopDocuments(raw_query, filter_pred);
    AddRequestToQueue(RequestType::PREDICATE_REQUEST, raw_query, request_resull);
    return request_resull;
}

void RequestQueue::AddRequestToQueue(RequestType request_type, const string& raw_query, vector<Document>& request_result)
{
    if (requests_.size() >= sec_in_day_)
        requests_.pop_front();
    requests_.push_back({request_type, request_result.size()});
}

int RequestQueue::GetNoResultRequests() const
{
    return count_if(requests_.begin(), requests_.end(),
                    [](const QueryResult& qr) -> bool
                    {
                        return !qr.result_counter;
                    });
}
