/*
 * LLM Runtime Simulator
 * Copyright (c) 2026 João Felipe De Souza
 * Licensed under the MIT License.
 */


#include <iostream>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <vector>
#include <algorithm>
#include "arena_allocator.hpp"

enum class Priority { INTERACTIVE, BATCH };
enum class CompletionReason { SUCCESS, EVICTED };
enum class Phase { PREFILL, DECODE };

struct SeqStats {
    int arrival_step;
    int completion_step = -1;
    size_t tokens_generated = 0;
    size_t decode_target = 512;
    size_t prefill_remaining = 0;
    Priority priority;
    Phase phase = Phase::PREFILL;
    CompletionReason reason = CompletionReason::SUCCESS;
};

int main() {

    size_t total_memory = 256ULL * 1024 * 1024;
    size_t page_size = 1ULL * 1024 * 1024;
    size_t bytes_per_token = 4096;

    size_t global_token_budget = 256;
    size_t prefill_chunk_size = 16;
    double pressure_threshold = 0.75;

    int sla_target = 300;
    int adaptive_weight = 2;
    int weight_min = 1;
    int weight_max = 6;

    ArenaAllocator allocator(total_memory, page_size, bytes_per_token);

    std::mt19937 rng(42);
    std::uniform_real_distribution<> prob(0.0, 1.0);
    std::uniform_int_distribution<int> input_dist(50, 300);

    std::unordered_set<int> active;
    std::unordered_map<int, SeqStats> stats;

    std::list<int> decode_int, decode_batch;
    std::list<int> prefill_int, prefill_batch;

    int next_id = 1;
    size_t total_tokens = 0;

    std::vector<int> recent_latencies;

    for (int step = 0; step < 1000; ++step) {

        // Arrival
        if (prob(rng) < 0.6 && allocator.memory_pressure() < pressure_threshold) {

            int id = next_id++;
            active.insert(id);

            Priority prio = (prob(rng) < 0.5)
                            ? Priority::INTERACTIVE
                            : Priority::BATCH;

            SeqStats s;
            s.arrival_step = step;
            s.priority = prio;
            s.prefill_remaining = input_dist(rng);

            stats[id] = s;

            if (prio == Priority::INTERACTIVE)
                prefill_int.push_back(id);
            else
                prefill_batch.push_back(id);
        }

        size_t tokens_given = 0;

        auto process_decode =
            [&](std::list<int>& lst, int weight) {

            for (auto it = lst.begin();
                 it != lst.end() && tokens_given < global_token_budget;) {

                int id = *it;

                if (!active.count(id)) {
                    it = lst.erase(it);
                    continue;
                }

                SeqStats& s = stats[id];

                int tokens_for_this_seq = 0;

                while (tokens_for_this_seq < weight &&
                       tokens_given < global_token_budget) {

                    if (!allocator.append_token(id))
                        break;

                    s.tokens_generated++;
                    total_tokens++;
                    tokens_given++;
                    tokens_for_this_seq++;

                    if (s.tokens_generated >= s.decode_target) {

                        allocator.free(id);
                        active.erase(id);
                        s.completion_step = step;
                        s.reason = CompletionReason::SUCCESS;

                        if (s.priority == Priority::INTERACTIVE)
                            recent_latencies.push_back(step - s.arrival_step);

                        it = lst.erase(it);
                        goto next_decode;
                    }
                }

                ++it;
            next_decode:;
            }
        };

        auto process_prefill =
            [&](std::list<int>& lst) {

            for (auto it = lst.begin();
                 it != lst.end() && tokens_given < global_token_budget;) {

                int id = *it;

                if (!active.count(id)) {
                    it = lst.erase(it);
                    continue;
                }

                SeqStats& s = stats[id];

                size_t chunk =
                    std::min(prefill_chunk_size,
                             s.prefill_remaining);

                for (size_t i = 0;
                     i < chunk && tokens_given < global_token_budget;
                     ++i) {

                    if (!allocator.append_token(id))
                        break;

                    s.prefill_remaining--;
                    total_tokens++;
                    tokens_given++;
                }

                if (s.prefill_remaining == 0) {

                    s.phase = Phase::DECODE;

                    if (s.priority == Priority::INTERACTIVE)
                        decode_int.push_back(id);
                    else
                        decode_batch.push_back(id);

                    it = lst.erase(it);
                }
                else {
                    ++it;
                }
            }
        };

        // Decode first
        process_decode(decode_int, adaptive_weight);
        process_decode(decode_batch, 1);

        // Prefill
        process_prefill(prefill_int);
        process_prefill(prefill_batch);

        // 🔥 Adaptive feedback every 100 steps
        if (step % 100 == 0 && !recent_latencies.empty()) {

            double avg = 0;
            for (int l : recent_latencies)
                avg += l;
            avg /= recent_latencies.size();

            if (avg > sla_target && adaptive_weight < weight_max)
                adaptive_weight++;

            if (avg < sla_target * 0.7 && adaptive_weight > weight_min)
                adaptive_weight--;

            std::cout << "Step " << step
                      << " | Avg Latency: " << avg
                      << " | Weight: " << adaptive_weight
                      << "\n";

            recent_latencies.clear();
        }
    }

    std::cout << "\nTotal Throughput: "
              << (double)total_tokens / 1000.0
              << " tokens/step\n";

    return 0;
}