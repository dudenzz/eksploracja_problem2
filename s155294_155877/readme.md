# Eclat w C++

## Kompilacja

Kod C++ należy najpierw skompilować w środowisku z zainstalowanym `pybind11`:
```bash
pip install pybind11
python setup.py build_ext --inplace
```

## Uruchomienie

solution.py działa jako wrapper i zakłada, że w folderze znajduje się plik config.py ze zmienną datapath, min_support oraz min_confidence

```bash
python solution.py
```

## Wyniki

**Czas** - wypisywany na standardowe wyjście (stdout).

**Reguły asocjacyjne** - zapisywane do pliku tekstowego rules_output.txt w głównym katalogu projektu.
Wypisywane logi lądują na wyjściu błędów (stderr).

## Prompt użyty do stworzenia bazowego rozwiązania

Zrealizuj poniższe zadanie wykorzystując algorytm eclat zaimplementowany w c++

Zadanie

W tym zadaniu wcielacie się w programistów, których zadaniem jest napisanie efektywnej metody odkrywania reguł asocjacyjnych. Pamiętajcie - liczy się ostateczny czas, więc aspekty takie jak dobór języka programowania, dobór algorytmów, sposób kompilacji i uruchomienia, zrównoleglenie, dobór bibliotek... mogą mieć wpływ na rozwiązanie.


Interfejs

Twoje rozwiązanie powinno implementować metodę solve(min_support, min_confidence, verbose=False)

Bez względu na język programowania, metoda ta jest owinięciem twojego rozwiązania w języku python.

Parametr verbose kontroluje to, czy wywołanie generuje informacje o wykonaniu.

Metoda powinna zwrócić listę reguł postaci:
A=>B, support = s, confidence = c

zakodowanych jako słownik:
{
'A' : List,
'B' : List,
'supp' : number,
'conf' : number
}