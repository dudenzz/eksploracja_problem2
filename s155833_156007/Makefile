
CXX = g++

CXXFLAGS = -std=c++17 -Wall -O3

WRAPPER_SRC = wrapper.cpp

all: python_module

python_module: $(WRAPPER_SRC)
	python build_module.py

python_module2: $(WRAPPER_SRC)
	python setup.py build_ext --inplace


