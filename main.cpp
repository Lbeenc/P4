// main.cpp (P4: compile)
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "scanner.h"
#include "parser.h"
#include "statSem.h"
#include "codeGen.h"

static std::string stripExtension(const std::string& s) {
    // If they pass "file.fs25s2", output should be "file.asm"
    auto dot = s.find_last_of('.');
    if (dot == std::string::npos) return s;
    return s.substr(0, dot);
}

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "Usage: compile [file]\n";
        return 1;
    }

    FILE* in = nullptr;
    std::string inName;
    bool usingFile = false;

    if (argc == 2) {
        usingFile = true;
        inName = argv[1];

        // P4 says "implicit extension" like P2/P3.
        // Try open exactly what they gave; if fails, try adding ".fs25s2"
        in = std::fopen(inName.c_str(), "r");
        if (!in) {
            std::string withExt = inName + ".fs25s2";
            in = std::fopen(withExt.c_str(), "r");
            if (!in) {
                std::cerr << "ERROR: cannot open input file '" << inName
                          << "' (also tried '" << withExt << "')\n";
                return 1;
            }
            inName = withExt;
        }
    }

    // init scanner (stdin if in == nullptr)
    initScanner(in);

    // build parse tree (P2)
    Node* root = parser();

    // validate variables (P3)
    staticSemantics(root);

    // produce target file name
    std::string outName;
    if (!usingFile) {
        outName = "a.asm";
    } else {
        outName = stripExtension(argv[1]) + ".asm";
    }

    // generate target (P4)
    try {
        generateTarget(root, outName);
    } catch (const std::exception& e) {
        std::cerr << "ERROR in P4: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
