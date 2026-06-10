#pragma once

#include "fp_types.hpp"

// Wczytuje CSV i koduje StockCode do int, zachowując mapowanie int -> oryginalny kod
Dataset read_data(const std::string &datapath);
