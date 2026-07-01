#pragma once

#include <vector>
#include <unordered_map>
#include <cstddef>
#include "page.hpp"

struct AllocationInfo {
    size_t num_tokens = 0;
    std::vector<size_t> logical_to_physical; // block index -> physical page index
};

class ArenaAllocator {
public:
    ArenaAllocator(size_t total_memory, size_t page_size, size_t bytes_per_token);

    bool append_token(int sequence_id);
    void free(int sequence_id);

    // Indirection lookup (simula fetch de KV block real)
    int get_physical_page(int sequence_id, size_t logical_token_index) const;

    size_t memory_used() const;
    size_t memory_free() const;
    size_t internal_fragmentation() const;

    double memory_pressure() const;
    size_t total_memory() const;
    size_t total_pages() const;

    double page_reuse_rate() const;

private:
    size_t page_size_;
    size_t bytes_per_token_;
    size_t tokens_per_page_;
    size_t total_pages_;

    std::vector<Page> pages_;
    std::vector<size_t> free_list_;
    std::unordered_map<int, AllocationInfo> allocations_;

    size_t total_page_allocations_ = 0;
    size_t total_page_reuses_ = 0;

    bool allocate_new_page(int sequence_id, AllocationInfo& info);
};