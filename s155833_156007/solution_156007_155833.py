import sys

try:
    # Próba importu skompilowanej biblioteki z C++ (zbudowanej przez pybind11)
    import s155833_156007.fpgrowth_fast as fpgrowth_fast
except ImportError:
    print("BŁĄD: Nie znaleziono modułu 'fpgrowth_fast'.")
    print("Upewnij się, że skompilowałeś kod C++ poleceniem: make python_module")
    sys.exit(1)

def solve(min_support, min_confidence, verbose=False):
    """
    Wrapper wywołujący natywny, zoptymalizowany algorytm C++.
    Zwraca listę słowników: [{'A': List, 'B': List, 'supp': float, 'conf': float}]
    """
    if verbose:
        print("Uruchamianie szybkiego silnika C++...")
        
    # Wywołanie dokładnie tej metody, którą zdefiniowaliśmy w wrapper.cpp
    return fpgrowth_fast.solve(min_support, min_confidence, verbose)