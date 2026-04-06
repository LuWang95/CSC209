# CSC209 Project: Parallel Monte Carlo π Estimator

## 1. Overview

This project implements a **parallel Monte Carlo simulation** to estimate the value of π using multiple worker processes.

The program follows a **parent–worker architecture**:

* The **parent process** distributes simulation work to multiple workers.
* Each **worker process** performs Monte Carlo trials independently.
* Workers send partial results back to the parent via pipes.
* The parent aggregates results and computes the final estimate.

This design demonstrates:

* Process creation using `fork()`
* Inter-process communication using `pipe()`
* Synchronization using `waitpid()`
* Robust error handling for system calls

---

## 2. How to Run

### Compile

```bash
make
```

### Run

```bash
./mc_pi <num_workers> <total_trials>
```

### Example

```bash
./mc_pi 4 1000000
```

* `num_workers`: number of worker processes (must be ≥ 3)
* `total_trials`: total number of Monte Carlo samples

---

## 3. Architecture

This program uses a **fixed worker pool**:

* 1 parent process
* N worker processes (N ≥ 3)

Each worker has two pipes:

* Parent → Worker (job dispatch)
* Worker → Parent (result collection)

### Data Flow

```
              parent
        /       |       \
   job pipe job pipe job pipe
      |         |         |
   worker0   worker1   worker2
      |         |         |
 result pipe result pipe result pipe
        \       |       /
              parent
```

---

## 4. Communication Protocol

Two message types are used.

### 4.1 Job Message (Parent → Worker)

```c
typedef struct {
    int job_id;
    long trials;
    unsigned int seed;
} job_msg_t;
```

**Fields:**

* `job_id`: unique identifier
* `trials`: number of simulations assigned
* `seed`: random seed for independent RNG

---

### 4.2 Result Message (Worker → Parent)

```c
typedef struct {
    int job_id;
    pid_t worker_pid;
    long trials_done;
    long inside_count;
} result_msg_t;
```

**Fields:**

* `job_id`: corresponding job
* `worker_pid`: process ID of worker
* `trials_done`: number of trials completed
* `inside_count`: number of points inside unit circle

---

### 4.3 Encoding

* Messages are transmitted as **fixed-size binary structs**
* Each message is sent using a single `write()`
* Reading is performed using `read_full()` to avoid partial reads

---

## 5. Implementation Details

### 5.1 Parent Responsibilities

* Parse command-line arguments
* Create pipes for each worker
* Spawn workers using `fork()`
* Distribute simulation tasks
* Collect results from all workers
* Compute final π estimate
* Clean up all resources

---

### 5.2 Worker Responsibilities

* Receive a job from parent
* Perform Monte Carlo simulation
* Send result back to parent
* Exit cleanly

---

### 5.3 Monte Carlo Method

Each worker generates random points `(x, y)` in `[0,1] × [0,1]`.

A point is inside the unit circle if:

```
x² + y² ≤ 1
```

π is estimated as:

```
π ≈ 4 × (total_inside / total_trials)
```

---

## 6. Error Handling

All critical system calls are checked:

* `pipe()`
* `fork()`
* `read()`
* `write()`
* `close()`
* `waitpid()`

### Strategy

* On failure:

  * Print error using `perror()`
  * Clean up open file descriptors
  * Terminate safely

* Worker failure:

  * Worker exits with `_exit(1)`
  * Parent continues collecting results

---

## 7. Resource Management

* All unused pipe ends are closed in both parent and child
* All file descriptors are properly closed
* All child processes are reaped using `waitpid()`
* No zombie processes remain

---

## 8. File Structure

```
project/
├── main.c
├── parent.c
├── parent.h
├── worker.c
├── worker.h
├── common.c
├── common.h
├── Makefile
└── README.md
```

---

## 9. Sample Output

```
Worker 0 (pid 12345): trials=250000 inside=196421
Worker 1 (pid 12346): trials=250000 inside=196102
Worker 2 (pid 12347): trials=250000 inside=196588
Worker 3 (pid 12348): trials=250000 inside=196301

Total trials: 1000000
Total inside: 785412
Estimated pi: 3.141648
```

---

## 10. Design Choices

* **Fixed worker pool**: simpler and ensures concurrency
* **Binary message format**: efficient and avoids parsing overhead
* **Independent RNG seeds**: ensures statistical correctness
* **One job per worker**: reduces protocol complexity

---

## 11. Limitations

* Each worker processes only one job
* No dynamic load balancing
* Blocking I/O (no select/poll)

---

## 12. Future Improvements

* Multiple job dispatch (true worker pool)
* Load balancing
* Non-blocking I/O
* Confidence interval computation

---

## 13. Summary

This project demonstrates core systems programming concepts:

* Process management
* Inter-process communication
* Resource safety
* Structured system design

The implementation prioritizes **clarity, correctness, and robustness**, aligning with CSC209 project requirements.
