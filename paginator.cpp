#include <iostream>
#include "paginator.h"

std::ostream& operator<<(std::ostream& out, const PageDescriptor& page_descriptor)
{
    for (const auto& curdoc : page_descriptor)
    {
        out << "{ document_id = " << curdoc.id ;
        out << ", relevance = " << curdoc.relevance;
        out << ", rating = " << curdoc.rating << " }";
    }
    return out;
}
