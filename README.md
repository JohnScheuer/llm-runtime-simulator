# LLM Runtime Simulator (C++20)

A systems-level simulator of modern LLM serving runtimes, modeling **KV-cache memory management**, **token scheduling**, **prefill/decode separation**, and **SLA-driven control** for capacity planning and tail-latency stability.

This is a production-inspired runtime modeling sandbox (not a toy allocator).

## What it models
Inspired by serving systems such as vLLM, TensorRT-LLM, and TGI:

- Paged KV cache allocation (fixed-size pages)
- Logical → physical block mapping (indirection table)
- Memory pressure + fragmentation metrics
- Pressure-driven eviction (LRU with workload-aware protection)
- Priority-aware token scheduling with a global token budget (tokens/step)
- Prefill vs decode phase separation (separate queues)
- Adaptive SLA controller (closed-loop QoS weight adjustment)
- Capacity planning experiments (throughput/latency vs token budget)

## Motivation
Production LLM serving requires balancing:
- KV-cache growth and memory fragmentation
- Throughput (tokens/sec)
- Tail latency (P95/P99)
- Fairness across tenants and SLA guarantees

Naive allocation/scheduling commonly leads to fragmentation, OOM failures, and unstable tail latency.
This simulator makes these tradeoffs **deterministic and reproducible**, enabling faster iteration on runtime policies.

## Architecture (high-level)
1) **Paged KV Cache Allocator**
- Token-level growth into fixed-size pages
- Logical → physical mapping with reuse tracking
- Internal fragmentation measurement
- Inspired by PagedAttention-style designs

2) **Memory Pressure Model**
`pressure = memory_used / total_memory`
Used for admission control, proactive eviction, and runtime stability.

3) **LRU Eviction Under Load**
- Batch sequences evicted first
- Interactive sequences protected
- Pressure-driven eviction loop

4) **Weighted Token Scheduler**
Each step simulates a GPU forward pass with a global token budget.
Interactive weight > batch weight for latency-sensitive decode prioritization.

5) **Prefill vs Decode Separation**
Separate queues prevent prefill bursts from blocking decode.

6) **Adaptive SLA Controller**
Closed-loop QoS controller:
- If `avg_latency > SLA` → increase interactive weight
- If `avg_latency < 0.7 * SLA` → decrease weight
Converges toward stable compute allocation under changing load.

## Example: capacity planning sweep
Budget,P95,Throughput 64,530,63.8 128,433,126.5 256,274,172.3 512,274,171.7

Key observations:
- A clear throughput knee around ~256 tokens/step
- Prefill shifts SLA requirements and can destabilize tail latency if not isolated
- Weighted scheduling reduces P95 under mixed interactive/batch load
- Increasing token budget beyond the knee does not increase throughput

## Build & run
```bash
cmake -S . -B build
cmake --build build
./build/llm_runtime_simulator
Requirements: C++20, CMake ≥ 3.16

License
MIT