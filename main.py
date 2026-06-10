import time
import statistics
import pandas as pd
from tabulate import tabulate 
import slow_solution_apriori
import slow_solution_fpgrowth
from s155829_156027 import rozwiazanie as solve1
from s156156_155941_155260 import fpgrowth_wrapper as solve2
import s155916_155864.our_soll as solve3
from s155833_156007 import solution_156007_155833 as solve4
from s155874_155974 import solution_155874_155974 as solve5
from s155937_155835_156014 import solution as solve6
from s158058_155077 import solution_158058_155077 as solve7
from s155294_155877 import solution as solve8
from s155093 import solution155093 as solve9
from s155898_156021_155934 import solution_155898_156021_155934 as solve10
from s155921_155198 import eclat_155921 as solve11
from s155922_155944 import solution_155922_155944 as solve12
from s155987_155956_156094 import fast_solution  as solve13
authors = [(("155829", "156027"), solve1), 
           (("156156", "155941", "155260"), solve2), 
           (("155916", "155864"), solve3),
           (("155833", "156007"), solve4),
           (("155874", "155974"), solve5),
           (("155937", "155835", "156014"), solve6),
           (("158058", "155077"), solve7), 
           (("155294", "155877"), solve8),
           (("155093"), solve9),
           (("155898", "156021", "155934"), solve10),
           (("155921", "155198"), solve11),
           (("155922", "155944"), solve12),
           (("155987", "155956", "156094"), solve13),
           ("Apriori (slow)", slow_solution_apriori),
           ("FP-Growth (slow)", slow_solution_fpgrowth)]


def benchmark_solution(solution_module, name, iterations=5, support=0.03, confidence=0.5):
    print(f"Rozpoczynam testy dla: {name}...")
    times = []
    
    for i in range(1, iterations + 1):
        start_time = time.perf_counter()
        result = solution_module.solve(support, confidence, verbose=True)
        end_time = time.perf_counter()
        
        duration = end_time - start_time
        times.append(duration)
        print(f"  Przebieg {i}/{iterations}: {duration:.4f}s (Znaleziono {len(result)} reguł)")
        
    return {
        "Algorytm": name,
        "Średnia [s]": round(statistics.mean(times), 4),
        "Min [s]": round(min(times), 4),
        "Max [s]": round(max(times), 4),
        "Odchylenie std.": round(statistics.stdev(times), 4) if len(times) > 1 else 0
    }

def run_comparison():
    import config
    MIN_SUPPORT = config.min_support 
    MIN_CONFIDENCE = config.min_confidence
    ITERATIONS = config.test_iters

    print("="*50)
    print(f"BENCHMARK (n={ITERATIONS})")
    print(f"Parametry: Support={MIN_SUPPORT}, Confidence={MIN_CONFIDENCE}")
    print("="*50)

    results = []

    # Test
    for sol in authors:
        try:
            apriori_stats = benchmark_solution(sol[1], sol[0], ITERATIONS, MIN_SUPPORT, MIN_CONFIDENCE)
            results.append(apriori_stats)
        except Exception as e:
            print(f"Błąd podczas testu {sol[0]}: {e}")

        print("-" * 30)

    # Wyświetlenie wyników
    print("\n" + "="*60)
    print("PODSUMOWANIE PORÓWNANIA")
    print("="*60)
    
    df_results = pd.DataFrame(results)
    print(tabulate(df_results, headers='keys', tablefmt='grid', showindex=False))

if __name__ == "__main__":
    run_comparison()