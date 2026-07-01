/*
 * LLM Runtime Simulator
 * Copyright (c) 2026 João Felipe De Souza
 * Licensed under the MIT License.
 */


#include "arena_allocator.hpp"

ArenaAllocator::ArenaAllocator(size_t total_memory, size_t page_size, size_t bytes_per_token)
    : page_size_(page_size),
      bytes_per_token_(bytes_per_token)
{
    tokens_per_page_ = page_size_ / bytes_per_token_;
    total_pages_ = total_memory / page_size_;

    pages_.reserve(total_pages_);
    for (size_t i = 0; i < total_pages_; ++i) {
        pages_.emplace_back(page_size_);
        free_list_.push_back(i);
    }
}

bool ArenaAllocator::allocate_new_page(int sequence_id, AllocationInfo& info) {
    if (free_list_.empty()) {
        return false; // OOM
    }

    size_t idx = free_list_.back();
    free_list_.pop_back();

    Page& page = pages_[idx];

    // Métricas de reuse
    total_page_allocations_++;
    if (page.reuse_count > 0) {
        total_page_reuses_++;
    }
    page.reuse_count++;

    page.allocated = true;
    page.sequence_id = sequence_id;

    info.logical_to_physical.push_back(idx);
    return true;
}

bool ArenaAllocator::append_token(int sequence_id) {
    AllocationInfo& info = allocations_[sequence_id];

    size_t current_block = info.num_tokens / tokens_per_page_;
    bool needs_new_block = (current_block >= info.logical_to_physical.size());

    if (needs_new_block) {
        if (!allocate_new_page(sequence_id, info)) {
            return false; // OOM
        }
    }

    info.num_tokens++;
    return true;
}

int ArenaAllocator::get_physical_page(int sequence_id, size_t logical_token_index) const {
    auto it = allocations_.find(sequence_id);
    if (it == allocations_.end()) return -1;

    size_t block_index = logical_token_index / tokens_per_page_;
    if (block_index >= it->second.logical_to_physical.size()) return -1;

    return static_cast<int>(it->second.logical_to_physical[block_index]);
}

void ArenaAllocator::free(int sequence_id) {
    auto it = allocations_.find(sequence_id);
    if (it == allocations_.end())
        return;

    for (size_t idx : it->second.logical_to_physical) {
        pages_[idx].allocated = false;
        pages_[idx].sequence_id = -1;
        free_list_.push_back(idx);
    }

    allocations_.erase(it);
}

size_t ArenaAllocator::memory_used() const {
    return (total_pages_ - free_list_.size()) * page_size_;
}

size_t ArenaAllocator::memory_free() const {
    return free_list_.size() * page_size_;
}

size_t ArenaAllocator::internal_fragmentation() const {
    size_t wasted = 0;

    for (const auto& [seq_id, info] : allocations_) {
        size_t allocated_bytes = info.logical_to_physical.size() * page_size_;
        size_t used_bytes = info.num_tokens * bytes_per_token_;
        wasted += (allocated_bytes - used_bytes);
    }

    return wasted;
}

double ArenaAllocator::memory_pressure() const {
    return static_cast<double>(memory_used()) /
           static_cast<double>(total_memory());
}

size_t ArenaAllocator::total_memory() const {
    return total_pages_ * page_size_;
}

size_t ArenaAllocator::total_pages() const {
    return total_pages_;
}

double ArenaAllocator::page_reuse_rate() const {
    if (total_page_allocations_ == 0) return 0.0;
    return static_cast<double>(total_page_reuses_) /
           static_cast<double>(total_page_allocations_);
}