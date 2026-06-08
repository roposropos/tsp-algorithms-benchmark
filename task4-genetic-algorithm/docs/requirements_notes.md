# Notatki z fotografii

## Zadanie 4

- Opracowac i zaimplementowac algorytm genetyczny (GA) albo algorytm mrowkowy (ACO) dla TSP.
- Dobrac parametry kontrolne wybranej metody tak, aby blad wzgledem optimum miescil sie w progach:
  - `n < 25`: `0%`
  - `24 < n < 74`: `50%`
  - `75 < n < 449`: `100%`
  - `450 < n < 2500`: `150%`
- Dażyć do uzyskania optimum.
- Wykonac badanie zlozonosci obliczeniowej i efektywnosci algorytmu dla roznych rozmiarow instancji, np. `14, 20, 25, 50, 75, 120, 170, 250, 450, 600, 900, 1300, 1800, 2500`.
- Oddzielnie zbadac instancje symetryczne i asymetryczne.
- Dane: TSPLIB oraz VLSI Waterloo.

## Program

- Jedno zadanie = jeden samodzielny program.
- Bez uniwersalnego menu wyboru algorytmu.
- Parametry uruchomienia i parametry algorytmu w tekstowym pliku konfiguracyjnym.
- Plik konfiguracyjny musi zawierac komentarze opisujace kolejne opcje.
- Jezyk kompilowany: C, C++, Julia albo Rust. Nie uzywac Pythona ani Javy jako programu rozwiazujacego zadanie.

## Raport

- Raport do zadania, maksymalnie 6 stron.
- Ma zawierac: tresc zadania, krotki opis metody z literatura, opis implementacji najlepiej schematem blokowym, informacje o danych, opis konfiguracji, hipotezy, procedure badawcza, tabele, wykresy, analize, wnioski i zrodla.
- Wyniki w tabelach i wykresach maja byc skumulowane i stanowic podstawe wnioskow.
- Formalnie: waskie marginesy 1,27 cm, TNR 11, interlinia 1,16, odstep po 6 pkt, wyrownanie do obu marginesow.
