# Task 4: Genetic Algorithm

C++17 implementation of a configurable hybrid Genetic Algorithm for TSP and ATSP. The solver uses tournament selection, OX crossover, mutation, elitism, nearest-neighbour seeds and local improvement.

## Run

```bash
make
make run
python3 scripts/summarize_results.py
```

The program creates the `results/tours/` directory automatically during a full experiment and exports the best routes in a TSPLIB-compatible `TOUR_SECTION` format. Generated tour files are intentionally excluded from this repository to keep the portfolio version compact.
