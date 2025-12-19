#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include <vector>
#include <unordered_set>
#include <fstream>
#include "node.h"

class CodeGen {
public:
    CodeGen(const std::string& outFile);
    void generate(node* root);

private:
    std::ofstream out;

    std::vector<std::string> storage;
    std::vector<std::string> code;

    std::unordered_set<std::string> declared;

    int tempCount = 0;

    std::string newTemp();
    void declare(const std::string& name);

    void emitCode(const std::string& s);
    void emitStorage(const std::string& s);

    void traverse(node* n);
};

#endif
