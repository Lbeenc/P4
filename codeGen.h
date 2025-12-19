#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include "node.h"

// Generate target assembly from parse tree.
// outAsmFile should be "a.asm" (stdin) or "<file>.asm" (file input).
void generateTarget(Node* root, const std::string& outAsmFile);

#endif
