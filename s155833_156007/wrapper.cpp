#include <string.h>
#include <cstring>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> 

// Dołączamy Twój kombajn z algorytmem
#include "fp-growth.cpp"

namespace py = pybind11;

// Ta funkcja tłumaczy wyniki C++ na listę słowników Pythona
py::list solve_wrapper(double min_support, double min_confidence, bool verbose = false) {
    AssociationRuleMiner miner;
    std::vector<Rule> rules = miner.solve(min_support, min_confidence, verbose);
    
    py::list py_rules;
    for (const auto& r : rules) {
        py::dict rule_dict;
        rule_dict["A"] = r.A;            
        rule_dict["B"] = r.B;
        rule_dict["supp"] = r.support;
        rule_dict["conf"] = r.confidence;
        py_rules.append(rule_dict);
    }
    return py_rules;
}

// Rejestracja modułu w Pythonie
PYBIND11_MODULE(fpgrowth_fast, m) {
    m.def("solve", &solve_wrapper, 
          py::arg("min_support"), 
          py::arg("min_confidence"), 
          py::arg("verbose") = false);
}