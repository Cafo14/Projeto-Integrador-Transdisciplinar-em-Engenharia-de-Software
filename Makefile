CXX ?= c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
LDLIBS = -lsqlite3
.PHONY: all run test clean
all: build/cupcake-server
build/cupcake-server: backend/main.cpp backend/domain.hpp backend/repository.hpp
	mkdir -p build data
	$(CXX) $(CXXFLAGS) backend/main.cpp -o $@ $(LDLIBS)
run: all
	./build/cupcake-server dist data/shop.sqlite 8080
test: all
	$(CXX) $(CXXFLAGS) tests/domain_test.cpp -o build/domain-tests $(LDLIBS)
	./build/domain-tests dist/products.json
clean:
	rm -f build/cupcake-server build/domain-tests
