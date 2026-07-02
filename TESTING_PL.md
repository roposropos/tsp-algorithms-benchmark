# Testowanie projektu

## Szybka weryfikacja

Najważniejszą komendą po sklonowaniu repozytorium jest:

```bash
make smoke
```

Ta komenda buduje i uruchamia krótkie testy dla wszystkich czterech etapów C++:

| Etap | Komenda wewnętrzna | Zakres testu |
| --- | --- | --- |
| 1 | `stage1_exact_heuristics/config_smoke.txt` | Małe instancje generowane, `bruteforce`, `nn`, `rnn`. |
| 2 | `stage2_branch_and_bound/config_smoke.txt` | Przykładowa macierz i tryb `lc`, oczekiwany koszt `30`. |
| 3 | `stage3_simulated_annealing/config/sa_smoke.txt` | Jedna instancja generowana, jedna TSP i jedna ATSP. |
| 4 | `stage4_genetic_algorithm/config/smoke.cfg` | Mały zestaw SYM, ATSP i VLSI. |

## Wynik ostatniej weryfikacji

Ostatnia lokalna weryfikacja przed spakowaniem repozytorium:

```text
make smoke
```

Status:

| Etap | Status | Przykładowy wynik |
| --- | --- | --- |
| Stage 1 | PASS | Instancje `n=6` i `n=7` zakończone dla `bruteforce`, `nn`, `rnn`. |
| Stage 2 | PASS | `example_matrix`, koszt najlepszej trasy `30`, optymalność potwierdzona. |
| Stage 3 | PASS | Smoke SA dla 3 instancji, CSV wynikowy wygenerowany poprawnie. |
| Stage 4 | PASS | Smoke GA dla `burma14`, `ftv33`, `xqf131`. |

## Pełniejsze uruchomienia

Po szybkim smoke te komendy uruchamiają właściwe zestawy eksperymentów:

```bash
make stage1-heuristics
make stage2-batch
make stage3-generated
make stage4-run
```

`stage4-run` jest najdłuższym przebiegiem, ponieważ wykonuje wiele powtórzeń na większych instancjach.

## Czysty stan przed commitem

Po testach warto wykonać:

```bash
make clean
```

`clean` usuwa binaria oraz pliki wynikowe smoke testów, które nie powinny trafić do commita.

