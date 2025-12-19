#ifndef CODEGEN_H
#define CODEGEN_H

#include "node.h"
#include <string>
#include <fstream>

// entry point from main
void generateTarget(Node* root, const std::string& outFile);

#endif
