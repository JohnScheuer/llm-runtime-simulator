# 📄 DESIGN DOCUMENT
## LLM Runtime Simulator

Author: João Felipe De Souza  
License: MIT  
Language: C++20  

---

# 1. Problem Statement

Modern LLM serving systems must balance:

- KV cache memory growth
- Token generation throughput
- Tail latency (P95 / P99)
- Fairness across priority classes
- SLA guarantees under load

In production environments, naive scheduling and memory allocation lead to:

- Fragmentation
- OOM failures
- Unbounded tail latency
- Throughput collapse under bursty workloads

This project models these tradeoffs in a controlled, deterministic simulation environment.

---

# 2. System Overview

The runtime simulator models:

1. A paged KV cache allocator
2. Logical → physical block mapping
3. Memory pressure tracking
4. LRU-based eviction
5. Priority-aware scheduling
6. Prefill vs Decode separation
7. Capacity planning experiments
8. SLA-driven adaptive compute allocation

Each simulation step represents a GPU forward pass.

---

# 3. Memory Model

## 3.1 Page-Based KV Allocation

Memory is divided into fixed-size pages.
total_memory
↓
pages[0..N-1]


Each sequence maintains:
logical_token_index → physical_page_index

This models PagedAttention-style block tables.

### Rationale

- Avoid large contiguous allocations
- Bound fragmentation
- Enable page reuse
- Simulate GPU KV paging behavior

---

## 3.2 Token-Level Growth

Tokens are appended incrementally: append_token(sequence_id)


If current page is full:

- Allocate new page
- Update indirection table

### Complexity

- Allocation: O(1)
- Append: amortized O(1)
- Free: O(pages_per_sequence)

---

## 3.3 Fragmentation Model

Internal fragmentation per sequence is bounded by: <= page_size


Total fragmentation: Σ (allocated_bytes - used_bytes)

This mirrors real KV paging tradeoffs.

---

# 4. Memory Pressure Model

Memory pressure is defined as: memory_pressure = memory_used / total_memory

Used for:

- Admission control
- Proactive eviction
- Runtime stability

When: memory_pressure >= threshold

Eviction is triggered.

---

# 5. Eviction Policy

LRU-based eviction with priority awareness:

1. Batch sequences evicted first
2. Interactive sequences evicted only if necessary

This models SLA-aware serving behavior.

### Tradeoff

- Protects latency-sensitive workloads
- Increases batch starvation under high pressure

---

# 6. Scheduling Model

Each simulation step represents a forward pass with: global_token_budget

Tokens are distributed in this order:

1. Decode – Interactive
2. Decode – Batch
3. Prefill – Interactive
4. Prefill – Batch

---

# 7. Prefill vs Decode Modeling

## 7.1 Prefill Phase

Simulates prompt processing:

- Bursty KV growth
- Chunked prefill (e.g., 16 tokens per step) prefill_remaining > 0 → Phase::PREFILL

Once completed: Phase → DECODE

---

## 7.2 Decode Phase

Incremental generation: weight_interactive > weight_batch

Models latency-sensitive token prioritization.

---

# 8. Weighted Scheduling

Interactive sequences receive higher decode weight:weight_interactive = W
weight_batch = 1

Minimum decode latency: Minimum decode latency: decode_target / weight_interactive

This demonstrates compute allocation tradeoffs.

---

# 9. Adaptive SLA Controller

Closed-loop feedback mechanism.

Every N steps: avg_latency = mean(recent_interactive_latencies)

Policy:if avg_latency > SLA:
weight_interactive++

if avg_latency < 0.7 * SLA:
weight_interactive--

Bounds:1 ≤ weight ≤ 6

---

## 9.1 Control Behavior

This acts as a proportional controller:

- Error = SLA - observed_latency
- Weight adjusts discretely
- Converges to stable equilibrium

Observed behavior:

- Stable fixed-point weight
- Minimal oscillation
- SLA compliance under sustained load

---

# 10. Capacity Planning Experiments

For budgets: {64, 128, 256, 512}

Metrics collected:

- Interactive P95 latency
- Throughput (tokens/step)
- Eviction rate

Observed patterns:

- Existence of throughput knee point
- SLA feasibility threshold
- Diminishing returns beyond compute saturation
- Prefill shifts required compute budget

---

# 11. Experimental Insights

## 11.1 Decode-Only vs Prefill+Decode

Prefill increases SLA budget requirements.

Without separation:
- Decode latency degraded by prefill bursts

With separation:
- Latency stabilized
- Compute isolation achieved

---

## 11.2 Weighted Scheduling

Increasing interactive weight:

- Reduces P95
- Increases eviction pressure
- Improves SLA compliance
- Does not always improve throughput

---

## 11.3 Knee Point Detection

Beyond certain budget: Throughput plateaus
Latency no longer improves

Indicates bottleneck shift from compute → arrival rate / memory.

---

# 12. Complexity Summary

| Component | Time Complexity |
|------------|----------------|
| Append token | O(1) amortized |
| Eviction | O(1) |
| Scheduling step | O(active_sequences) |
| Prefill chunk | O(chunk_size) |

Simulation runtime: O(steps × active_sequences)

---

# 13. Limitations

This is a simulator, not a real GPU runtime.

Not modeled:

- GPU occupancy
- Kernel fusion
- Tensor parallelism
- Cross-device communication
- Real KV tensor size scaling by layers/heads

---

# 14. Future Extensions

- Multi-GPU sharding
- Tensor parallel memory modeling
- Page compaction strategies
- Beam search copy-on-write blocks
- Autoscaling simulation
- PID-based controller
- Real trace-driven workload simulation

---

# 15. Conclusion

This project demonstrates:

- Memory allocator design
- Runtime scheduling tradeoffs
- SLA modeling
- Capacity planning
- Adaptive compute allocation
- Production-inspired LLM runtime architecture

It serves as a sandbox for exploring LLM serving system behavior under controlled constraints.

---

# 👤 Author

João Felipe De Souza  
C++ Systems Engineer | ML Infrastructure | Runtime Design

---

# 📜 License

MIT License  
Copyright (c) 2026 João Felipe De Souza