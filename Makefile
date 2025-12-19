CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

OBJS = main.o parser.o node.o printTree.o scanner.o statSem.o codeGen.o

compile: $(OBJS)
	$(CXX) $(CXXFLAGS) -o compile $(OBJS)

main.o: main.cpp scanner.h parser.h statSem.h codeGen.h token.h
parser.o: parser.cpp parser.h node.h scanner.h token.h
node.o: node.cpp node.h token.h
printTree.o: printTree.cpp printTree.h node.h token.h
scanner.o: scanner.cpp scanner.h token.h
statSem.o: statSem.cpp statSem.h node.h token.h
codeGen.o: codeGen.cpp codeGen.h node.h token.h

clean:
	rm -f *.o compile *.asm
