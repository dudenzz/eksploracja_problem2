Informacje jak uruchomić nasze rozwiązanie
Made by 155921 and 155198

Domyślnym rozwiązaniem jest Eclat, jeżeli jednak okaże się, że rozwiązanie jest niepoprawne, zaimplementowane zostało dodatkowe rozwiązanie przy wykorzystaniu FP-Growth.

1. Uruchomić skrypt ./data/download.py
2. Sprawdzić, bądź ustawić path w config
3. Uruchomić skrypt generujący plik do wykonania. 
python setup.py build_ext --inplace
lub
py setup.py build_ext --inplace
4. Pliki potrzebne do rozwiązania to wrapper python, główna funkcjonalność cpp oraz plik pyd. Metoda Eclat poprzedzona jest nazwą eclat oraz indeksem 155921, a FP-Growth C++ nazwą oraz idneksem 155198.
5. Wykonanie main.py
    a. Eclat
    ```
    try:
        eclat155921_stats = benchmark_solution(eclat_155921, "dEclat 155921", ITERATIONS, MIN_SUPPORT, MIN_CONFIDENCE)
        results.append(eclat155921_stats)
    except Exception as e:
        print(f"Błąd podczas testu dEclat 155921: {e}")
    ```
    
    b. FP-Growth CPP
    ```
    try:
        fpgrowth155198_stats = benchmark_solution(fpgrowth_155198, "FP-Growth 155198", ITERATIONS, MIN_SUPPORT, MIN_CONFIDENCE)
        results.append(fpgrowth155198_stats)
    except Exception as e:
        print(f"Błąd podczas testu 155198: {e}")
    ```