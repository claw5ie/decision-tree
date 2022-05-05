WFLAGS=-Wall -Wextra -pedantic
OFLAGS=-std=c++11 -g
OUTPUT=main.out

all: debug_build

debug_build: ./main.cpp
	g++ $(WFLAGS) $(OFLAGS) -o $(OUTPUT) $^

release_build: ./main.cpp
	g++ -O3 -D NDEBUG -o $(OUTPUT) $^
