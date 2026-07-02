# Stage 2 - Branch and Bound

C++ implementation of Branch and Bound for TSP with three OPEN-set strategies.

## Modes

| Mode | Strategy |
| --- | --- |
| `bfs` | Breadth-first search. |
| `dfs` | Depth-first search. |
| `lc` | Least-cost / best-first search using the lower bound. |

## Run

```bash
make
./tsp_bnb config.txt
./tsp_bnb batch_config.txt
```
