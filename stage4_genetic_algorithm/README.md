# Stage 4 - Genetic Algorithm

C++ Genetic Algorithm benchmark for larger TSP, ATSP and VLSI instances.

## Build

```bash
make
```

## Run

```bash
./tsp_ga config/experiment.cfg
```

## Implemented Operators

| Operator | Implementation |
| --- | --- |
| Selection | Tournament selection. |
| Crossover | Order crossover (`OX`). |
| Mutation | Swap, reverse and insertion moves. |
| Local search | 2-opt for symmetric instances, randomized improvement for ATSP. |

## Outputs

- `results/experiment_results.csv`
- `results/summary_by_instance.csv`
- `results/summary_by_group.csv`
- `plots/gap_by_instance.png`
- `plots/time_by_dimension.png`
- `plots/limit_usage.png`
