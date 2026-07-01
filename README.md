# 🔥 LLM Runtime Simulator

A systems-level simulation of modern LLM serving runtimes, modeling memory management, scheduling, SLA control, and capacity planning inspired by production LLM infrastructure.

---

## 🚀 Overview

This project simulates key architectural components used in large-scale LLM serving systems such as:

- vLLM
- TensorRT-LLM
- HuggingFace TGI
- Production GPU-backed inference runtimes

It models:

✅ Paged KV cache allocation  
✅ Logical → physical block mapping  
✅ Memory pressure & fragmentation  
✅ LRU eviction under load  
✅ Priority-aware token scheduling  
✅ Prefill vs Decode phase separation  
✅ Capacity planning experiments  
✅ Adaptive SLA-driven scheduling  

This is not a toy allocator —  
it is a runtime systems modeling sandbox.

---

## 🧠 Motivation

Serving large language models in production requires balancing:

- Memory usage (KV cache growth)
- Throughput (tokens/sec)
- Tail latency (P95/P99)
- Fairness across tenants
- SLA guarantees

Naive allocation or scheduling leads to:

- Fragmentation
- OOM failures
- Unbounded tail latency
- Poor GPU utilization

This simulator explores those tradeoffs in a deterministic, reproducible way.

---

## 🏗 Architecture

### 1️⃣ Paged KV Cache Allocator

- Fixed-size memory pages
- Token-level growth
- Logical → physical indirection table
- Page reuse tracking
- Internal fragmentation measurement

Inspired by the PagedAttention design.

---

### 2️⃣ Memory Pressure Model

```text
memory_pressure = memory_used / total_memory