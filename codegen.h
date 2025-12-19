#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include "node.h"

// Generate assembly for the whole program parse tree.
// outFile should be "a.asm" (keyboard input) or "<file>.asm" (file input).
void generateTarget(Node* root, const std::string& outFile);

#endif
