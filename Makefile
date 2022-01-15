CXX=g++
CPPFLAGS=-I.
CFLAGS=-O3

all: woodokuAI

woodokuAI: main.o
	$(CXX) -o woodokuAI main.o

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(wildcard *.o) woodokuAI
