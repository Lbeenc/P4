#ifndef CODEGEN_H
#define CODEGEN_H

#include <ostream>
#include "node.h"

void generateTarget(Node* root, std::ostream& out);

#endif
