#ifndef CODEGEN_H
#define CODEGEN_H

#include "node.h"
#include <string>

// Entry point for P4 code generation
// root = parse tree from P2
// outName = name of .asm file to generate
void generateCode(Node* root, const std::string& outName);

#endif
