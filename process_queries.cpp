#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <numeric>
#include <execution>

#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries)
{
    vector<vector<Document>> resull(queries.size());
    transform(execution::par, queries.begin(), queries.end(), resull.begin(),
              [&search_server](string query)
              {
                return search_server.FindTopDocuments(query);
              });
    return resull;
}

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries)
{
    auto intermediate_container = ProcessQueries(search_server, queries);
    return accumulate(intermediate_container.begin(), intermediate_container.end(), list<Document>(),
                      [](list<Document>& accumulator_list, const vector<Document>& current_vector) -> list<Document>&
                      {
                            accumulator_list.splice(accumulator_list.end(), list<Document>(current_vector.begin(), current_vector.end()));
                            return accumulator_list;
                      });
}
