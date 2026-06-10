# Instrukcja

```
    python -m venv venv
    pip install -r "requirements.txt"
    cd data
    python data/download.py
```

# Zadanie

W tym zadaniu wcielacie się w programistów, których zadaniem jest napisanie efektywnej metody odkrywania reguł asocjacyjnych. Pamiętajcie - liczy się ostateczny czas, więc aspekty takie jak dobór języka programowania, dobór algorytmów, sposób kompilacji i uruchomienia, zrównoleglenie, dobór bibliotek... mogą mieć wpływ na rozwiązanie.

Czas przetwarzania zbioru będzie miał bezpośrednie przełożenie na liczbę uzystkanych punków.

Algorytmy będą testowane z użyciem zbioru, który można pobrać za pomocą skryptu w katalogu data.

# Materiały 

Do dyspozycji masz rozwiązania problemu napisane w języku python:
 - slow_solution_apriori.py : implementuje "od zera" algorytm apriori
 - slow_solution_fpgrowth.py implementuje "od zera" algorytm FP-Growth

# Interfejs

Twoje rozwiązanie powinno implementować metodę
```solve(min_support, min_confidence, verbose=False)```

Bez względu na język programowania, metoda ta jest owinięciem twojego rozwiązania w języku python.

Parametr verbose kontroluje to, czy wywołanie generuje informacje o wykonaniu.

Metoda powinna zwrócić listę reguł postaci:

A=>B, support = s, confidence = c

zakodowanych jako słownik:
```
{
    'A' : List,
    'B' : List,
    'supp' : number,
    'conf' : number
}
```

# Wyniki

Skala (brana pod uwagę jest średnia z 10 przebiegów):
0s-1s 25 pkt
1s-2s 22 pkt
2s-4s 19 pkt
4s-10.32s 15 pkt
10.32s-55.21s 10pkt


| Algorytm | Średnia [s] | Min [s] | Max [s] | Odchylenie std. |
| :--- | :--- | :--- | :--- | :--- |
| ('155829', '156027') | 1.4064 | 1.3806 | 1.4835 | 0.0339 |
| ('156156', '155941', '155260') | 0.6968 | 0.3185 | 4.0147 | 1.1658 |
| ('155916', '155864') | 1.7618 | 1.7277 | 1.7934 | 0.0196 |
| ('155833', '156007') | 1.1627 | 1.1524 | 1.187 | 0.011 |
| ('155874', '155974') | 3.4787 | 3.3465 | 3.5327 | 0.0527 |
| ('155937', '155835', '156014') | 0.2977 | 0.2709 | 0.3145 | 0.013 |
| ('158058', '155077') | 0.4853 | 0.476 | 0.5212 | 0.0132 |
| ('155294', '155877') | 0.278 | 0.2669 | 0.2976 | 0.0086 |
| 155093 | 0.9872 | 0.974 | 1.0081 | 0.011 |
| ('155898', '156021', '155934') | 0.8039 | 0.725 | 0.9101 | 0.0701 |
| ('155921', '155198') | 1.6272 | 1.5993 | 1.7449 | 0.043 |
| ('155922', '155944') | 1.0559 | 1.037 | 1.0845 | 0.0161 |
| ('155987', '155956', '156094') | 0.6102 | 0.6038 | 0.6261 | 0.0063 |
| Apriori (slow) | 55.2179 | 49.6266 | 65.0373 | 4.6504 |
| FP-Growth (slow) | 10.3215 | 9.8426 | 12.3247 | 0.7421 |