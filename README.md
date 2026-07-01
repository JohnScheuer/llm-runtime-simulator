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

Used for:

Admission control
Proactive eviction
Runtime stability

3️⃣ LRU Eviction
Batch sequences evicted first
Interactive sequences protected
Pressure-driven eviction loop

4️⃣ Weighted Token Scheduler
Each step simulates a GPU forward pass with a global token budget:
Interactive weight > Batch weight
Models latency-sensitive decode prioritization.

5️⃣ Prefill vs Decode Separation
Real LLM serving consists of two phases:

Prefill (prompt processing)
Decode (incremental generation)
Separate queues prevent prefill bursts from blocking latency-sensitive decode.

6️⃣ Adaptive SLA Controller
Closed-loop QoS controller:
If avg_latency > SLA → increase weight
If avg_latency < 0.7 * SLA → decrease weight
Simulates dynamic compute allocation in production systems.

📊 Example Capacity Planning Output
Budget,P95,Throughput
64,530,63.8
128,433,126.5
256,274,172.3
512,274,171.7

Key Insights
There exists a throughput knee point (~256 tokens/step)
Prefill significantly shifts SLA requirements
Weighted scheduling drastically reduces P95
Adaptive scheduling converges to stable compute allocation
Increasing budget beyond the knee point does not increase throughput

📈 What This Demonstrates
✅ Memory allocator design
✅ Runtime scheduling tradeoffs
✅ SLA modeling
✅ Capacity planning methodology
✅ Closed-loop QoS control
✅ Production-inspired architecture

🛠 Build
cmake -S . -B build
cmake --build build
./build/llm_runtime_simulator

Requires:

C++20
CMake ≥ 3.16

📁 Suggested Repository Structure
llm-runtime-simulator/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── DESIGN.md
│
├── include/
│   ├── arena_allocator.hpp
│   └── page.hpp
│
├── src/
│   ├── arena_allocator.cpp
│   └── main.cpp
│
└── experiments/

🚀 Future Work
Multi-GPU sharding simulation
Tensor parallel memory modeling
Page compaction strategies
Beam search copy-on-write
Autoscaling trigger modeling
PID-based adaptive scheduler


👤 Author
João Felipe De Souza
C++ Systems Engineer | ML Infrastructure | LLM Runtime Design

This project was built as a research-oriented systems engineering exercise inspired by modern LLM serving architectures.

📜 License
This project is licensed under the MIT License.

Copyright (c) 2026 João Felipe De Souza

See the LICENSE file for details.