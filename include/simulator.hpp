#pragma once

#include "arena_allocator.hpp"

class Simulator {
public:
    Simulator(ArenaAllocator& allocator);

    void run(size_t steps);

private:
    ArenaAllocator& allocator_;
};