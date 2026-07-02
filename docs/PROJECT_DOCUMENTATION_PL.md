# Dokumentacja projektu

## Cel projektu

`tsp-algorithms-benchmark` to czteroetapowe badanie algorytmów dla problemu komiwojażera (`TSP`) oraz jego wariantu asymetrycznego (`ATSP`). Projekt pokazuje przejście od metod dokładnych i prostych heurystyk do metaheurystyk uruchamianych na większych instancjach. Właściwe programy benchmarkowe są zaimplementowane w C++.

Repozytorium zostało odświeżone pod publikację na GitHubie: stare katalogi robocze zostały rozdzielone na spójne etapy, dodano wspólne README, dokumentację, checklistę publikacji, testy smoke, raporty PDF, wykresy i rootowy `Makefile`.

## Zakres funkcjonalny

| Etap | Katalog | Funkcjonalności |
| --- | --- | --- |
| 1 | `stage1_exact_heuristics` | C++: generowanie grafów pełnych, wczytywanie TSPLIB, brute force, RAND, nearest neighbor, repetitive nearest neighbor, zapis wyników CSV. |
| 2 | `stage2_branch_and_bound` | C++: Branch and Bound dla TSP, trzy strategie przeglądania OPEN: BFS, DFS i LC, statystyki drzewa wyszukiwania oraz batch runner. |
| 3 | `stage3_simulated_annealing` | Program C++ dla Simulated Annealing, obsługa danych generowanych, TSPLIB i ATSP, konfiguracja tekstowa, wyniki szczegółowe i agregacje. |
| 4 | `stage4_genetic_algorithm` | Program C++ dla algorytmu genetycznego, OX crossover, selekcja turniejowa, mutacje, lokalne ulepszanie, benchmark TSPLIB/VLSI, eksport tras. |

## Dane wejściowe

Projekt zawiera kilka typów danych:

| Typ danych | Użycie |
| --- | --- |
| Proste macierze wag | Małe przykłady i testy Branch and Bound. |
| Instancje generowane | Badanie wpływu rozmiaru `n` na czas działania i błąd. |
| TSPLIB `.tsp` | Klasyczne symetryczne instancje TSP. |
| TSPLIB `.atsp` | Asymetryczne instancje ATSP. |
| Waterloo VLSI | Większe instancje użyte w etapie algorytmu genetycznego. |

## Etap 1: metody bazowe

Plik główny: `stage1_exact_heuristics/src/main.cpp`.

Zaimplementowane algorytmy:

| Algorytm | Opis |
| --- | --- |
| `bruteforce` | Przegląda wszystkie permutacje miast przy ustalonym mieście startowym. Daje wynik optymalny, ale skaluje się silniowo. |
| `rand` | Losuje permutacje i wybiera najlepszą z określonej liczby prób. |
| `nn` | Buduje trasę zachłannie, wybierając najbliższe nieodwiedzone miasto. |
| `rnn` | Uruchamia nearest neighbor z wielu startów i wybiera najlepszy wynik. |

Przykłady:

```bash
cd stage1_exact_heuristics
make
./tsp_stage1 config_bruteforce.txt
./tsp_stage1 config_heuristics.txt
```

Wyniki:

- `bf_generated.csv`
- `heuristics_tsplib_fixed.csv`
- wykresy `rys*.png`

## Etap 2: Branch and Bound

Plik główny: `stage2_branch_and_bound/src/main.cpp`.

Program implementuje Branch and Bound z trzema strategiami obsługi zbioru OPEN:

| Tryb | Znaczenie |
| --- | --- |
| `bfs` | Przeszukiwanie wszerz. |
| `dfs` | Przeszukiwanie w głąb. |
| `lc` | Least-cost / best-first według dolnego ograniczenia. |

Program mierzy:

- koszt najlepszej trasy,
- czas działania,
- liczbę węzłów wygenerowanych i rozwiniętych,
- liczbę odcięć przez ograniczenie,
- maksymalny rozmiar zbioru OPEN,
- informację, czy optymalność została dowiedziona.

Przykłady:

```bash
cd stage2_branch_and_bound
make
./tsp_bnb config.txt
./tsp_bnb batch_config.txt
```

Wykresy z poprzednich eksperymentów są zachowane jako pliki PNG. Sam benchmark i batch runner działają w C++.

## Etap 3: Simulated Annealing

Katalog: `stage3_simulated_annealing`.

Program jest napisany w C++ i uruchamiany przez `Makefile`.

Najważniejsze elementy:

| Element | Opis |
| --- | --- |
| `src/`, `include/` | Kod programu i moduły benchmarku. |
| `config/` | Pliki konfiguracyjne z parametrami SA. |
| `data/` | Instancje generowane, TSPLIB, ATSP oraz plik referencyjny. |
| `results/` | Wyniki szczegółowe i agregacje CSV. |

Przykłady:

```bash
cd stage3_simulated_annealing
make
make run-generated
make run-mixed
```

Konfiguracja obejmuje m.in. temperaturę początkową, współczynnik chłodzenia, limit iteracji, limit czasu, typ sąsiedztwa oraz sposób tworzenia rozwiązania początkowego.

## Etap 4: Genetic Algorithm

Katalog: `stage4_genetic_algorithm`.

Program rozwiązuje większe instancje TSP/ATSP metodą genetyczną i mierzy błąd względem znanych wartości optymalnych lub najlepszych potwierdzonych.

Zastosowane mechanizmy:

| Mechanizm | Opis |
| --- | --- |
| Populacja początkowa | Część osobników pochodzi z nearest neighbor, część jest losowa. |
| Selekcja | Turniejowa. |
| Krzyżowanie | Order crossover (`OX`). |
| Mutacje | Swap, reverse i insertion. |
| Lokalne ulepszanie | 2-opt dla instancji symetrycznych oraz losowe ulepszanie dla ATSP. |
| Stop | Limit generacji, limit czasu i stagnacja bez poprawy. |

Przykłady:

```bash
cd stage4_genetic_algorithm
make
./tsp_ga config/experiment.cfg
```

Najważniejsze wyniki są w:

- `results/experiment_results.csv`
- `results/summary_by_instance.csv`
- `results/summary_by_group.csv`
- `plots/gap_by_instance.png`
- `plots/time_by_dimension.png`
- `plots/limit_usage.png`

## Komendy z katalogu głównego

Rootowy `Makefile` udostępnia skróty:

```bash
make help
make smoke
make stage1-build
make stage2
make stage3-build
make stage4-build
```

Dłuższe eksperymenty, zwłaszcza `stage4-run`, mogą potrwać, bo wykonują wiele powtórzeń na większych instancjach.

## Testowanie

Szybka weryfikacja wszystkich etapów:

```bash
make smoke
```

Smoke testy budują i uruchamiają każdy z czterech programów C++ na małych konfiguracjach. Szczegóły znajdują się w `docs/TESTING_PL.md`.

## Wyniki i raporty

W katalogu `docs/assets/` znajdują się wybrane wykresy użyte w głównym README. W katalogu `docs/reports/` znajdują się raporty PDF z poszczególnych etapów:

- `stage1_exact_heuristics_report.pdf`
- `stage2_branch_and_bound_report.pdf`
- `stage3_simulated_annealing_report.pdf`
- `stage4_genetic_algorithm_report.pdf`

## Zmiany wykonane przy odświeżeniu

- Uporządkowano projekt do jednego katalogu `tsp-algorithms-benchmark`.
- Nadano czytelne nazwy katalogom etapów.
- Przeniesiono rdzeń etapów 1 i 2 do C++.
- Dodano szybkie smoke testy C++ dla wszystkich czterech etapów oraz dokumentację `docs/TESTING_PL.md`.
- Usunięto z finalnej wersji stare archiwa, środowisko `.venv`, `__pycache__`, buildy i pełne kopie robocze.
- Dodano spójne README w stylu portfolio.
- Dodano polską dokumentację projektu.
- Dodano rootowy `Makefile` z komendami startowymi.
- Dodano `.gitignore` i `.editorconfig`.
- Zebrano raporty PDF w `docs/reports/`.

## Rekomendowany opis repozytorium na GitHubie

Krótki opis:

```text
Four-stage benchmark of exact algorithms, heuristics and metaheuristics for TSP and ATSP problems.
```

Tematy:

```text
tsp, atsp, algorithms, branch-and-bound, simulated-annealing, genetic-algorithm, tsplib, cpp, benchmark
```
