CXX=g++
CFLAGS=-O3

all: woodokuAI

woodokuAI: main.o piece.o game.o printutil.h woodoku_client.o
	$(CXX) -o woodokuAI main.o piece.o game.o woodoku_client.o

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(wildcard *.o) woodokuAI
