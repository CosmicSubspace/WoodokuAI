CXX=g++
CFLAGS=-O3

all: woodokuAI

woodokuAI: main.o piece.o game.o printutil.h
	$(CXX) -o woodokuAI main.o piece.o game.o -lpthread

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(wildcard *.o) woodokuAI
