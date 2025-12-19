#ifndef CODEGEN_H
#define CODEGEN_H

#include <ostream>
#include "node.h"

// NOTE: Node is capitalized (matches node.h)
void generateTarget(Node* root, std::ostream& out);

#endif
