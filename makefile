WFLAGS=-Wall -Wextra -pedantic
OFLAGS=-std=c++11 -g
OUTPUT=main.out

all: debug_build

debug_build: ./main.cpp ./src/Table.cpp ./src/Category.cpp ./src/DecisionTree.cpp ./src/Utils.cpp
	g++ $(WFLAGS) $(OFLAGS) -o $(OUTPUT) $^

release_build: ./main.cpp ./src/Table.cpp ./src/Category.cpp ./src/DecisionTree.cpp ./src/Utils.cpp
	g++ -O3 -D NDEBUG -o $(OUTPUT) $^
