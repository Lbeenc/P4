#ifndef SCANNER_H
#define SCANNER_H
#include <cstdio>
#include "token.h"


// Initialize scanner with an input FILE* (defaults to stdin if nullptr)
void initScanner(FILE* in);


// Get next token (one at a time)
Token scanner();


// Tester: repeatedly call scanner() and print tokens per spec
int testScanner();


#endif // SCANNER_H
