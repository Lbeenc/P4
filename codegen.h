#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include "node.h"

// Generate assembly target into outFile (e.g., a.asm or file.asm)
void generateTarget(Node* root, const std::string& outFile);

#endif
