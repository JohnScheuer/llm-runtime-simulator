#pragma once
#include <cstddef>

struct Page {
    size_t size;
    bool allocated;
    int sequence_id;
    size_t reuse_count;  // contador de reuso

    Page(size_t s)
        : size(s),
          allocated(false),
          sequence_id(-1),
          reuse_count(0) {}
};