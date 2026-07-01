# 📄 DESIGN DOCUMENT — LLM Runtime Simulator

Author: João Felipe De Souza  
License: MIT  
Language: C++20  

---

## 1. Problem Statement

Modern LLM serving systems must balance:

- KV-cache memory growth
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

## 2. System Overview

The runtime simulator models:

1. A paged KV-cache allocator
2. Logical → physical block mapping (indirection table)
3. Memory pressure tracking
4. LRU-based eviction with workload awareness
5. Priority-aware token scheduling
6. Prefill vs decode separation
7. Capacity planning sweeps
8. SLA-driven adaptive compute allocation (closed-loop controller)

Each simulation step represents a GPU forward-pass “time slice” with a global token budget.

---

## 2.1 Definitions & Units

To keep the simulator deterministic and implementation-friendly, core metrics are expressed in **simulation steps**.

- **Simulation step:** one discrete runtime tick, representing a GPU forward pass time slice.
- **Token budget (B):** maximum tokens that can be processed per step (global).
- **Latency metric:** measured in **steps** (convertible to milliseconds by choosing an external `step_duration_ms`).
  - **Interactive decode latency:** steps from admission to token production for interactive decode tokens.
- **Throughput:** tokens processed per step (convertible to tokens/sec via `step_duration_ms`).

---

## 3. Memory Model

### 3.1 Page-Based KV Allocation

Memory is divided into fixed-size pages: total_memory ↓ pages[0..N-1]

Each sequence maintains a logical→physical mapping: logical_token_index → physical_page_index

This models a PagedAttention-style block table.

#### Rationale
- Avoid large contiguous allocations
- Bound internal fragmentation
- Enable page reuse
- Simulate GPU KV paging behavior

---

### 3.2 Token-Level Growth

Tokens are appended incrementally:

`append_token(sequence_id)`

If the current page becomes full:
- Allocate a new page
- Update the indirection table (logical→physical)

#### Complexity
- Page allocation: **O(1)** (free-list/bitmap)
- Append token: **amortized O(1)**
- Free sequence: **O(pages_per_sequence)**

---

### 3.3 Fragmentation Model

Internal fragmentation per sequence is bounded by:

`<= page_size`

Total fragmentation:

`Σ (allocated_bytes - used_bytes)`

This mirrors real KV paging tradeoffs: paging prevents large contiguous allocations but introduces bounded internal waste.

---

### 3.4 Allocator Invariants

- Each logical block maps to **at most one** physical page at any time.
- A physical page is owned by **at most one** sequence at any time.
- The free page set is tracked by a single source of truth (free-list/bitmap).
- Eviction must release all pages belonging to a sequence and remove its mapping entries.

---

## 4. Memory Pressure Model

Memory pressure is defined as:

`pressure = memory_used / total_memory`

Used for:
- Admission control
- Proactive eviction
- Runtime stability

When:

`pressure >= threshold`

eviction is triggered.

---

## 5. Eviction Policy

LRU-based eviction with priority awareness:

1. Batch sequences are evicted first
2. Interactive sequences are evicted only if necessary

This models SLA-aware serving behavior.

#### Tradeoff
- Protects latency-sensitive workloads
- Can increase batch starvation under high pressure

#### Complexity note
LRU victim selection can be O(1) with a linked list + hash/index structure, but page release is proportional to the sequence footprint.

---

## 6. Scheduling Model

Each simulation step represents a forward pass with a **global token budget** `B`.

Tokens are served in priority order:

1. Decode – Interactive  
2. Decode – Batch  
3. Prefill – Interactive  
4. Prefill – Batch

This ordering prevents prefill bursts from blocking latency-sensitive decode.

---

## 7. Prefill vs Decode Modeling

### 7.1 Prefill Phase
Simulates prompt processing:
- Bursty KV growth
- Chunked prefill (e.g., 16 tokens per step)

If `prefill_remaining > 0` → `Phase::PREFILL`  
Once completed → `Phase::DECODE`

### 7.2 Decode Phase
Incremental generation:
- Latency-sensitive decode prioritization
- Typically `weight_interactive > weight_batch`

---

## 8. Weighted Scheduling

Interactive decode receives higher weight:

- `weight_interactive = W`
- `weight_batch = 1`

### 8.1 Budget Allocation Rule (informal)

At each step, the scheduler allocates the global token budget `B` across classes by priority and weight:

- Decode classes are served before prefill classes.
- Within decode, interactive receives weight `W` vs batch weight `1`.
- Within a class, sequences are served round-robin (one token at a time) until the class allocation is exhausted.

This provides a simple approximation of production decode prioritization with bounded per-step work.

---

## 9. Adaptive SLA Controller

Closed-loop feedback mechanism.

Every `N` steps:
- `avg_latency = mean(recent_interactive_decode_latencies)`

Policy: if avg_latency > SLA: W++

if avg_latency < 0.7 * SLA: W--

Bounds:
- `W_min = 1`
- `W_max = 6`

---

### 9.1 Control Behavior

This acts as a coarse discrete proportional controller:

- Error: `e = avg_latency - SLA`
- Discrete actuation: `W ← clamp(W + sign(e), W_min, W_max)`
- Anti-windup via clamping at bounds

Observed behavior:
- Stable fixed-point `W` under sustained load
- Minimal oscillation in steady state
- Improved SLA compliance under mixed workloads

---

## 10. Capacity Planning Experiments

Sweeps over token budgets `{64, 128, 256, 512}`.

Metrics collected:
- Interactive P95 latency (steps)
- Throughput (tokens/step)
- Eviction rate
- Memory pressure/fragmentation (if exported)

Observed patterns:
- Existence of throughput knee point
- SLA feasibility threshold
- Diminishing returns beyond compute saturation
- Prefill shifts required compute budget upward

---

## 11. Experimental Insights

### 11.1 Decode-Only vs Prefill+Decode

Prefill increases SLA budget requirements.

Without separation:
- Decode latency degrades during prefill bursts

With separation:
- Decode latency stabilizes
- Compute isolation is achieved via queue separation + priority ordering

---

### 11.2 Weighted Scheduling

Increasing interactive weight:
- Reduces interactive P95
- Can increase eviction pressure (more decode work, longer residency)
- Improves SLA compliance
- Does not always increase throughput (throughput may saturate at the knee)

---

### 11.3 Knee Point Detection

Beyond a certain budget:
- Throughput plateaus
- Latency no longer improves

This indicates bottleneck shift from compute budget → arrival rate and/or memory pressure dynamics.

---

## 12. Complexity Summary

| Component | Time Complexity |
|----------|------------------|
| Append token | O(1) amortized |
| Select eviction victim (LRU) | O(1) |
| Evict sequence (free pages + mapping cleanup) | O(pages_per_sequence) |
| Scheduling step | O(active_sequences) |
| Prefill chunk | O(chunk_size) |

Simulation runtime:
`O(steps × active_sequences)` (dominant term)

---

## 13. Reproducibility

- Simulator runs should be reproducible via a fixed RNG seed (configurable).
- Experiments export CSV outputs for plotting and regression comparisons (budget sweeps, latency, eviction).

---

## 14. Limitations

This is a simulator, not a real GPU runtime.

Not modeled:
- GPU occupancy
- Kernel fusion
- Tensor parallelism
- Cross-device communication
- Real KV tensor size scaling by layers/heads

---

## 15. Future Extensions

- Multi-GPU sharding simulation
- Tensor parallel memory modeling
- Page compaction strategies
- Beam search copy-on-write blocks
- Autoscaling simulation
- PID-based controller
- Trace-driven workload simulation (production replay)

---

## 16. Conclusion

This project demonstrates:
- Memory allocator design (paged KV-cache, fragmentation metrics)
- Runtime scheduling tradeoffs (prefill/decode separation, weighted scheduling)
- SLA modeling via closed-loop control
- Capacity planning methodology
- Production-inspired runtime architecture

It serves as a sandbox for exploring LLM serving behavior under controlled constraints.

---

## 👤 Author

João Felipe De Souza  
C++ Systems Engineer | ML Infrastructure | Runtime Design

---

## 📜 License

MIT License  
Copyright (c) 2026 João Felipe De Souza