// statSem.h
#ifndef STATSEM_H
#define STATSEM_H

#include "node.h"

// Run static semantics on the parse tree.
// On error: prints "ERROR in P3: ..." and exits.
// On warning(s): prints "WARNING in P3: ..." lines and returns normally.
void staticSemantics(Node* root);

#endif
