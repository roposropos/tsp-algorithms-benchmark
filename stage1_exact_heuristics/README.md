# Stage 1 - Exact Search and Constructive Heuristics

C++ baseline for TSP experiments.

## Algorithms

| Algorithm | Description |
| --- | --- |
| `bruteforce` | Checks all permutations with a fixed start city. |
| `rand` | Samples random tours and keeps the best one. |
| `nn` | Nearest neighbor from city `0`. |
| `rnn` | Repetitive nearest neighbor from multiple starts. |

## Run

```bash
make
./tsp_stage1 config_bruteforce.txt
./tsp_stage1 config_heuristics.txt
```

## Outputs

- `bf_generated.csv`
- `heuristics_tsplib_fixed.csv`
- `rys*.png`
