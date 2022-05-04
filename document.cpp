#include "document.h"

Document::Document() : id(0), relevance(0), rating(0)
{}
Document::Document(int inp_id, double inp_relevance, int inp_rating) :
    id(inp_id), relevance(inp_relevance), rating(inp_rating)
{}
