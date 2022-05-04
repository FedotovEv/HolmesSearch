#pragma once
#include <iostream>
#include <vector>
#include <iterator>
#include "document.h"

using PageDescriptor = std::vector<Document>; // Список документов, относяшихся к странице

template <typename Iterator>
class Paginator
{
public:
    class PageIterator : public std::iterator<std::bidirectional_iterator_tag, const PageDescriptor>
    {
    public:
        friend class Paginator;
        PageIterator(const Paginator *pagin_p, int page_num) : pag_p(pagin_p)
        {
            if (page_num < 0)
                current_page_number = 0;
            else if (page_num >= static_cast<int>(pag_p->pages_descriptors_.size()))
                current_page_number = pag_p->pages_descriptors_.size();
            else
                current_page_number = page_num;
        }
        PageIterator& operator++()
        {
            if (current_page_number < static_cast<int>(pag_p->pages_descriptors_.size()))
                ++current_page_number;
            return *this;
        }

        PageIterator& operator--()
        {
            if (current_page_number > 0)
                --current_page_number;
            return *this;
        }

        const PageDescriptor& operator*() const
        {
            return pag_p->pages_descriptors_.at(current_page_number);
        }

        bool operator==(const PageIterator& pag_it)
        {
            return pag_p == pag_it.pag_p && current_page_number == pag_it.current_page_number;
        }

        bool operator!=(const PageIterator& pag_it)
        {
            return !(*this == pag_it);
        }
    private:
        const Paginator *pag_p;
        int current_page_number;
    };
    friend class PageIterator;

    Paginator(Iterator& result_begin, Iterator& result_end, int page_size)
    {
        auto curdoc_it = result_begin;
        while (curdoc_it != result_end)
        {
            PageDescriptor cur_page_desc;
            for (int docs_in_page = 0; docs_in_page < page_size; ++docs_in_page)
                if (curdoc_it != result_end)
                {
                    cur_page_desc.push_back(*curdoc_it);
                    ++curdoc_it;
                }
                else
                    break;
            pages_descriptors_.push_back(cur_page_desc);
        }
    }

    PageIterator begin() const
    {
        return {this, 0};
    }

    PageIterator end() const
    {
        return {this, static_cast<int>(pages_descriptors_.size())};
    }

    unsigned size()
    {
        return pages_descriptors_.size();
    }
private:
    // Вектор page_ содержит список описателей страниц типа PageDescriptor
    std::vector<PageDescriptor> pages_descriptors_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size)
{
    return Paginator(begin(c), end(c), page_size);
}

std::ostream& operator<<(std::ostream& out, const PageDescriptor& page_descriptor);
