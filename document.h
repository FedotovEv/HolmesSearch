#pragma once

struct Document
{
    Document();
    Document(int inp_id, double inp_relevance, int inp_rating);
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};
