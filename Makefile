CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

OBJS = main.o parser.o node.o printTree.o scanner.o statSem.o

statSem: $(OBJS)
	$(CXX) $(CXXFLAGS) -o statSem $(OBJS)

main.o: main.cpp parser.h scanner.h token.h statSem.h
parser.o: parser.cpp parser.h node.h scanner.h token.h
node.o: node.cpp node.h token.h
printTree.o: printTree.cpp printTree.h node.h token.h
scanner.o: scanner.cpp scanner.h token.h
statSem.o: statSem.cpp statSem.h node.h token.h

clean:
	rm -f *.o statSem
