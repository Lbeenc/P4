// main.cpp (P4: compile)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "scanner.h"
#include "parser.h"
#include "statSem.h"
#include "codeGen.h"

static const char* EXT = ".fs25s2";

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "Usage: compile [file]\n";
        return 1;
    }

    FILE* in = nullptr;
    std::string baseName;
    std::string outName = "a.asm";

    if (argc == 2) {
        baseName = argv[1];
        std::string inName = baseName + EXT;

        in = std::fopen(inName.c_str(), "r");
        if (!in) {
            std::cerr << "ERROR: cannot open input file '" << inName << "'\n";
            return 1;
        }

        outName = baseName + ".asm";
    }

    // scanner reads stdin if in == nullptr
    initScanner(in);

    // P2: build parse tree
    Node* root = parser();

    // P3: static semantics (must print to stdout and exit on error)
    staticSemantics(root);

    // P4: codegen to output file
    std::ofstream out(outName);
    if (!out) {
        std::cerr << "ERROR: cannot open output file '" << outName << "'\n";
        return 1;
    }

    generateTarget(root, out);
    out.close();

    return 0;
}
