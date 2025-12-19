CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

OBJS = main.o parser.o node.o scanner.o statSem.o codegen.o

compile: $(OBJS)
	$(CXX) $(CXXFLAGS) -o compile $(OBJS)

main.o: main.cpp parser.h scanner.h token.h statSem.h codegen.h
parser.o: parser.cpp parser.h node.h scanner.h token.h
node.o: node.cpp node.h token.h
scanner.o: scanner.cpp scanner.h token.h
statSem.o: statSem.cpp statSem.h node.h token.h
codegen.o: codegen.cpp codegen.h node.h token.h

clean:
	rm -f *.o compile
