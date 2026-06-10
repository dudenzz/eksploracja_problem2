# Projekt 2: Reguły Asocjacyjne (ECLAT C++) 155937, 155835 i 156014


## Kompilacja
**Developer PowerShell for VS**:
```
cl /O2 /GL /openmp /std:c++17 /EHsc /MD /LD fast_arules.cpp /Fe:fast_arules.dll /link /LTCG
```

## Uruchomienie
```
python solution.py
```

## Wyniki
- **Konsola (stdout)**: Wyświetla czas obliczeń w milisekundach oraz informację o zapisie pliku.
- **Plik**: Reguły są zapisywane do rules.json.

## Konfiguracja (config.py)
- `min_support`: wsparcie.
- `min_confidence`: ufność.
- `datapath`: ścieżka do CSV.