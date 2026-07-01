# Task 2: Branch and Bound

Python implementation of an exact Branch and Bound solver for TSP with three search strategies:

- `bfs`: FIFO queue;
- `dfs`: LIFO stack;
- `lc`: priority queue ordered by the smallest lower bound.

The benchmark records runtime, generated and expanded nodes, pruning statistics and maximum OPEN size.

```bash
python3 task2_bnb_tsp.py config.txt
python3 batch_task2.py batch_config.txt
```
