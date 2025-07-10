# Classic Process Problems

This repository contains solutions to classic process synchronization problems:
- Producer-Consumer
- Readers-Writers
- Dining Philosophers

## Table of Contents
1. [Producer-Consumer](#producer-consumer)
2. [Readers-Writers](#readers-writers)
3. [Dining Philosophers](#dining-philosophers)

---

### Producer-Consumer

#### Explanation
In this problem, there are two types of processes: **Producer** and **Consumer**.
Additionally, there is a buffer with `N` positions: `{b1, ..., bN}`.

- The **Producer** process places a value in the buffer.
- The **Consumer** process retrieves a value from the buffer.

#### Restrictions
1. Two or more producers cannot produce at the same time—only one can.
2. Two or more consumers cannot consume at the same time—only one can.
3. If the buffer is full, producers must wait.
4. If the buffer is empty, consumers must wait.

#### Execution
This repository contains `producer-consumer/simulate.c`. To compile and execute:
```sh
git clone https://github.com/eloyUA/Classic-Process-Problems
cd Classic-Process-Problems/producer-consumer
gcc simulate.c -o simulate
./simulate <key> <nProducers> <minTimeP> <maxTimeP> <nConsumers> <minTimeC> <maxTimeC>
```

**Arguments:**
- `<key>`: The key for creating semaphores and shared memory.
- `<nProducers>`: Number of producer processes.
- `<minTimeP>` and `<maxTimeP>`: Time range `[minTimeP, maxTimeP]` (in microseconds) for producing a value.
- `<nConsumers>`: Number of consumer processes.
- `<minTimeC>` and `<maxTimeC>`: Time range `[minTimeC, maxTimeC]` (in microseconds) for consuming a value.

---

### Readers-Writers

#### Explanation
In this problem, there are two types of processes: **Reader** and **Writer**.

- The **Reader** process reads from any position in the buffer.
- The **Writer** process writes to any position in the buffer.

#### Restrictions
1. If there are `M` readers (`nReaders = M`), then no writer can write (`nWriters = 0`).
2. If there is at least one writer (`nWriters = 1`), then no readers can read (`nReaders = 0`).
3. Number of the writers writing <= 1

#### Execution
This repository contains `reader-writer/simulatePriorityReaders.c` and `reader-writer/simulatePriorityWriters.c`. To compile and execute:
```sh
git clone https://github.com/eloyUA/Classic-Process-Problems
cd Classic-Process-Problems/reader-writer
gcc simulatePriorityReaders.c -o simulatePriorityReaders
gcc simulatePriorityWriters.c -o simulatePriorityWriters
./simulatePriorityReaders <key> <nReaders> <minTimeR> <maxTimeR> <nWriters> <minTimeW> <maxTimeW>
./simulatePriorityWriters <key> <nReaders> <minTimeR> <maxTimeR> <nWriters> <minTimeW> <maxTimeW>
```

**Arguments:**
- Similar to the Producer-Consumer problem, but adapted to Readers-Writers.

---

### Dining Philosophers

#### Explanation
The **Dining Philosophers** problem is a synchronization problem that illustrates the challenges of allocating limited resources without causing deadlock.

- There are `N` philosophers sitting at a round table.
- Each philosopher alternates between **thinking** and **eating**.
- There are `N` forks, one between each pair of philosophers.
- A philosopher needs both the left and right fork to eat.

#### Restrictions
1. A philosopher can only pick up one fork at a time.
2. If a philosopher picks up the left fork, they must wait for the right fork to be available before eating.
3. If all philosophers pick up the left fork simultaneously, a **deadlock** may occur.

#### Execution
This repository contains `dining-philosophers/simulate.c`. To compile and execute:
```sh
git clone https://github.com/eloyUA/Classic-Process-Problems
cd Classic-Process-Problems/dining-philosophers
gcc simulate.c -o simulate
./simulate <key> <nPhilosophers> <minTimeThink> <maxTimeThink> <minTimeEat> <maxTimeEat>
```

**Arguments:**
- `<key>`: The key for creating semaphores and shared memory.
- `<nPhilosophers>`: Number of philosopher processes.
- `<minTimeThink>` and `<maxTimeThink>`: Time range `[minTimeThink, maxTimeThink]` (in microseconds) for thinking.
- `<minTimeEat>` and `<maxTimeEat>`: Time range `[minTimeEat, maxTimeEat]` (in microseconds) for eating.
