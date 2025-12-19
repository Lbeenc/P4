// main.cpp  (P3: statSem)
#include <cstdio>
#include <iostream>
#include "scanner.h"
#include "parser.h"
#include "statSem.h"

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "Usage: statSem [file]\n";
        return 1;
    }

    FILE* in = nullptr;

    if (argc == 2) {
        in = std::fopen(argv[1], "r");
        if (!in) {
            std::cerr << "ERROR: cannot open input file '" << argv[1] << "'\n";
            return 1;
        }
    }

    // initialize scanner (stdin if in == nullptr)
    initScanner(in);

    // build parse tree (P2)
    Node* root = parser();

    // P3: run static semantics on the tree
    staticSemantics(root);

    // On success: no output (unless there were warnings)
    return 0;
}
