CXX=g++
CFLAGS=-O3

all: woodokuAI

woodokuAI: main.o piece.o game.o
	$(CXX) -o woodokuAI main.o piece.o game.o -lpthread

main.o: main.cpp woodoku_client.h printutil.h
	$(CXX) -c main.cpp -o main.o $(CFLAGS)

piece.o: piece.cpp piece.h
	$(CXX) -c piece.cpp -o piece.o $(CFLAGS)

game.o: game.cpp game.h
	$(CXX) -c game.cpp -o game.o $(CFLAGS)

clean:
	rm -f $(wildcard *.o) woodokuAI
