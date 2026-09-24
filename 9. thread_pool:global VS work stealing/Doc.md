Copy this into `pool_lab/README.md` (or `phase9.md`). It is the lab write-up, not extra code.

---

# Phase 9 — Tiny thread pool: global queue vs work stealing

## 1. What this phase is

Phases 1–8 measured **the machine**: caches, lines, threads, atomics, locks, affinity.

Phase 9 puts that into one program you already care about: a **thread pool**.

A pool is not “start a `std::thread` per task.” It is:

```text
N long-lived workers
    ↑
tasks sit in queues
    ↑
producer submits work
```

The **queue design** decides who fights for which lock, which cache line, and whether an idle core sits empty while another core’s queue is full.

You implement **two** pools that run the **same** tasks, then time them at **three task sizes**.

| Pool | Queue | Sync |
|------|--------|------|
| **Global** | one `deque` | one `std::mutex` |
| **Steal** | one `deque` **per worker** | one mutex **per worker**; idle workers **steal** |

That is the whole phase.

---

## 2. Why it exists (the brief)

```text
global queue  →  worker threads          // first design
```

then

```text
producer ──┬── worker 0  local queue
           ├── worker 1  local queue
           └── worker 2  local queue
                 ↑ steal from a neighbour if empty
```

Plus:

- atomic **task counters** (how you know the job is done)
- **cache-line padding** on worker metadata (Phase 4/5)
- **different task sizes** (when queue cost matters vs when `burn` hides it)

This is the same idea as a high-performance pool, cut down to one file you can reason about.

---

## 3. What a “task” is

A task is not a `std::function`. It is:

```text
struct Task { uint64_t work; };
burn(work);   // same tight loop as affinity_lab
```

`work` is **how expensive one task is**:

| `work` | name | what dominates wall time |
|--------|------|---------------------------|
| 20 | tiny | mutex, steal attempts, atomics, lines |
| 2000 | medium | mix |
| 80000 | large | the `burn` itself |

**8000 tasks**, **8 workers**. Same counts for both pools so the table is fair.

If every task is huge, both pools look the same: you are not measuring the pool. If every task is tiny, you are measuring **contention**, which is the point.

---

## 4. Pool A — global mutex queue

```text
                    ┌─────────────┐
  producer fills    │  one deque  │
                    │  one mutex  │
                    └──────┬──────┘
                           │ lock / pop
              ┌────────────┼────────────┐
           worker 0     worker 1     worker 7
```

**Loop (each worker):**

1. Lock the **only** mutex.  
2. If the queue has a task, pop front. Unlock.  
3. `burn`.  
4. `done.fetch_add(1)`.  
5. If the queue is empty, unlock, `yield`, try again until `done == ntasks`.

**What you are rehearsing**

- **Mutex (Phase 7):** every pop is the same lock. High contention when tasks are tiny.  
- **One cache line:** the deque and mutex live in one place. Eight cores bounce that line (Phase 4).  
- **Simple and correct.** No steal bugs. Often **slower** when many cores take tiny tasks.

Empty queue + `t.work == 0`: we used `work == 0` as “did not pop.” Real tasks use `work` of 20 / 2000 / 80000, never 0.

---

## 5. Pool B — local queues + steal

```text
  tasks assigned round-robin at start
       0 1 2 3 4 5 6 7 0 1 2 …
       ↓
  worker 0 q    worker 1 q    …    worker 7 q
  (own mutex)   (own mutex)        (own mutex)
       │              │
       │ empty? steal front of someone else
```

### 5.1 `Worker` (the padded struct)

```text
struct alignas(128) Worker {
    mutex mu;
    deque<Task> q;
};
```

`alignas(128)` (your M5 line) so **worker 0’s mutex is not on the same line as worker 1’s**. Packed `Worker`s would **false-share** (Phases 4–5): stealing from `ws[3]` would invalidate `ws[2]`’s line.

`sizeof(Worker)` is printed so you can see it is at least 128 B, not “mutex + deque packed into 40 B.”

### 5.2 Own queue = LIFO (pop **back**)

You take the task you **just** pushed / the newest one. That is often still **warm in your L1**.

### 5.3 Steal = FIFO (pop **front**)

You take the **oldest** task from the victim. That is less likely to be the victim’s hot working set. Classic pool folklore: **owner LIFO, thief FIFO**.

### 5.4 Search order

```text
1. pop_own(me)
2. steal_from(me+1), steal_from(me+2), … wrap around
3. if nobody has work:
      if done >= ntasks → exit
      else yield and retry
```

**Work stealing** means: idle cores **pull**, they do not wait for a global dispatcher. Load imbalance (one queue still full) gets drained by thieves.

**Cost:** extra locks, extra probes, extra coherence on **other** workers’ mutexes. If all queues are empty at once, everyone spins/`yield`s until `done` catches up.

---

## 6. Atomic task counter

```text
atomic<uint64_t> done{0};
// after each burn:
done.fetch_add(1, memory_order_release);
// idle check:
done.load(memory_order_acquire) >= ntasks
```

- **Not** a lock. One integer (Phase 6 / 7 “atomic counter”).  
- **release** on the add, **acquire** on the load: idle workers should **see** that the last task finished (happens-before).  
- This is the **stop condition**. Without it, empty queues look like “deadlock” vs “still starting.”

Do not use this counter as the **queue**. The queue holds `Task`s; `done` only counts finished burns.

---

## 7. How a run is glued (`main`)

```text
for work in {20, 2000, 80000}:
    t_global = run_global(8, 8000, work)
    t_steal  = run_steal (8, 8000, work)
    print work, t_global, t_steal
```

Steal pool: **fill queues first** (round-robin), **then** start threads. No live producer in this tiny version. The brief’s “producer in the middle” is the same shape; tasks are just enqueued before `join`.

---

## 8. How this maps to earlier labs

| Lab | What Phase 9 reuses |
|-----|---------------------|
| **cpu_lab** | 8 workers ≤ 10 cores; extra threads would oversubscribe (Phase 3) |
| **cache_lab** | `burn` is sequential; tiny tasks never hide memory |
| **thread_lab** | more workers ≠ always faster if they share one mutex |
| **cache_line / false_sharing_int** | `alignas(128)` on `Worker` |
| **c++_memo_model** | `fetch_add` / `load` orders on `done` |
| **mutex_spinlock_atomic** | global pool = mutex; steal = many small mutexes; `done` = atomic |
| **affinity_lab** | we do **not** pin here; the OS still places workers on P/E |

---

## 9. What to expect (do not pick a “winner” before you run)

**Tiny tasks:** global often **slower** (one lock, eight cores). Steal can be faster (mostly uncontended local pop) **or** messy (everyone steals).  

**Large tasks:** times **converge**. The pool is a small tax on a long `burn`. If they do not converge, something else is wrong (oversubscription, P/E, one thread stuck).

**Steal is not always faster.** It is a **different bottleneck**: many locks + steal traffic vs one lock + one line.

Same rule as Phase 6: **faster is not “the correct pool for every product.”** A server with huge tasks does not need steal. A GUI thread with 1 µs jobs might.

---

## 10. What this file is *not*

- Not a lock-free **Chase–Lev** deque (real high-perf steal).  
- Not `std::function` / lambdas as tasks (allocation would drown tiny `work`).  
- Not a live producer after start (no `submit()` API).  
- Not affinity-pinned workers.  
- Not exception-safe or `std::jthread`.

Enough to **see** global vs steal and padding. Not enough to ship.

---

## 11. Build and run

```bash
cd pool_lab   # or whatever folder you used
c++ -std=c++17 -O2 -pthread main.cpp -o phase9
./phase9
```

Use **Release / `-O2`**. Debug makes `burn` dominate even at `work=20`.

---

## 12. How to read the table

```text
task burn     global mutex(s)    steal(s)
20                 ?                ?
2000               ?                ?
80000              ?                ?
```

1. Look at **`work=20` first** — that is the pool.  
2. Look at **`work=80000`** — if both ~equal, the lesson is “granularity.”  
3. If steal ≫ global on tiny tasks, thieves may be fighting (yield storm). Note it; do not “fix” until you understand the loop.  
4. `Worker sizeof` should be **≥ 128**. If it were ~64 and packed in a `vector`, you would be back in Phase 4.

---

## 13. One-sentence summary

**Phase 9 is: same 8000 burns, two ways to hand them to 8 cores — one global lock, or padded local queues plus steal — so you can see that a thread pool is a cache- and lock-design, not a `std::thread` API.**

---

If you want this saved as a file in the repo, switch to Agent mode and say where to put it. After you run `./phase9`, paste the table and we can read it the same way as the other phases.