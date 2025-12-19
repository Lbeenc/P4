#ifndef CODEGEN_H
#define CODEGEN_H

#include <ostream>
#include "node.h"

// Generate target assembly into the provided output stream.
void generateTarget(Node* root, std::ostream& out);

#endif
