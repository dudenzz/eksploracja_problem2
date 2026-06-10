import subprocess
import os
import config

def solve(min_support, min_confidence, verbose=False):
    # Określenie ścieżki do pliku wykonywalnego
    executable = os.path.join(os.getcwd(), "gr.exe" if os.name == 'nt' else "gr")
    
    # Kompilacja tylko jeśli plik nie istnieje (z optymalizacją -O3)
    if not os.path.exists(executable):
        if not os.path.exists("regulyAsocjacyjneZgodne.cpp"):
            print("Błąd: Nie znaleziono pliku .cpp do kompilacji!")
            return []
        
        try:
            print("Kompilacja kodu C++...")
            subprocess.run(["g++", "-O3", "-std=c++17", "regulyAsocjacyjneZgodne.cpp", "-o", executable], check=True)
        except subprocess.CalledProcessError:
            print("Błąd kompilacji!")
            return []

    # Mapowanie verbose na parametr dla C++
    v_param = "1" if verbose else "0"
    
    try:
        # Uruchomienie programu C++
        result = subprocess.run(
            [executable, str(min_support), str(min_confidence), v_param, config.datapath],
            capture_output=True, 
            text=True, 
            encoding='utf-8'
        )
        
        if result.returncode != 0:
            print(f"Błąd wykonania C++: {result.stderr}")
            return []

        rules = []
        
        # Szybkie parsowanie wyjścia z C++
        lines = result.stdout.strip().split('\n')
        for line in lines:
            # Interesują nas tylko linie zawierające reguły
            if '}=>{' in line:
                try:
                    parts = line.split('}=>{')
                    # Lewa strona (Antacedent)
                    lhs = [i.strip() for i in parts[0].strip('{').split(',') if i.strip()]
                    
                    # Prawa strona + Metryki
                    rhs_parts = parts[1].split('},')
                    rhs = [i.strip() for i in rhs_parts[0].split(',') if i.strip()]
                    
                    # Support i Confidence (rozdzielone przecinkami na końcu linii w C++)
                    metrics = rhs_parts[1].split(',')
                    
                    rules.append({
                        'A': set(lhs),
                        'B': set(rhs),
                        'supp': float(metrics[0]),
                        'conf': float(metrics[1])
                    })
                except (IndexError, ValueError):
                    continue
        print(rules)
        # ZWRACAMY TYLKO LISTĘ REGUŁ
        return rules

    except Exception as e:
        print(f"Wystąpił nieoczekiwany błąd: {e}")
        return []

if __name__ == "__main__":
    # Teraz 'wyniki' to po prostu lista słowników
    wyniki = solve(config.min_support, config.min_confidence, verbose=False)
    
    print(f"Liczba wygenerowanych reguł: {len(wyniki)}")
    
    # Opcjonalne wypisanie kilku pierwszych reguł dla testu
    for r in wyniki[:5]:
        print(f"Reguła: {r['A']} -> {r['B']} (S: {r['supp']:.4f}, C: {r['conf']:.4f})")