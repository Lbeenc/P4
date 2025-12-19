# P3

Curtis Been — cjb2m8
CS4280 – Principles of Programming Languages
Project P3 – Static Semantics
Due: December 7, 2025

1. Overview

This project implements the static semantics phase of the compiler.
Using the parse tree generated in P2, the program checks variable definitions and uses according to the P3 specification.

The project supports the Global Scope Option (100%), where all variables belong to one global symbol table.

The program executable name is:

statSem

2. Files Included
main.cpp          – program entry, handles invocation and calls static semantics  
scanner.cpp/h     – lexical analyzer from P1  
token.h           – token definitions  
parser.cpp/h      – parser from P2, builds the parse tree  
node.cpp/h        – parse tree node structures  
printTree.cpp/h   – kept for debugging (not called in grading)  
statSem.cpp/h     – NEW: static semantics processing  
Makefile          – builds the statSem executable  
README.md         – project documentation  

3. How to Compile

Run:

make


This generates:

./statSem


To clean object files:

make clean

4. Program Invocation

Valid invocations:

./statSem
./statSem <file>


Examples:

./statSem test.fs25s2


Invalid invocations (e.g., too many arguments) are not graded.

5. Static Semantics Implemented

The semantics follow the P3 specification:

Variable Definition

Any identifier appearing under <vars> or <varList> is a definition.

Variable Use

Any identifier appearing in any other node is a use.

Rules
Errors (exit immediately)

Variable used before it is defined

ERROR in P3: variable 'id_x' used before definition on line 5


Variable defined more than once

ERROR in P3: variable 'id_x' redefined on line 7 (first defined on line 3)

Warnings (do NOT exit)

Variable defined but never used

WARNING in P3: variable 'id_x' defined on line 2 but never used

If no errors occur:

The program prints nothing.

This matches: “Other good programs not generating any errors, nothing will be reported.”

6. Static Semantics Design Summary
Symbol Table (STV)

A vector of:

name      – identifier string, e.g., "id_1"
defLine   – line where defined
used      – boolean: true if used at least once

Insert (definitions)

Called while traversing <vars> or <varList>

Errors on redeclaration

Verify (uses)

Called on any identifier outside of <vars>

Errors if identifier not yet defined

CheckVars()

After full tree traversal, prints warnings for unused variables

7. Main Program Flow

Open input file (or stdin)

Initialize scanner

Build parse tree using parser()

Run static semantics:

staticSemantics(root);


If an error occurs → print error and exit

If warnings occur → print warnings and continue

8. Assumptions

The scanner and parser behave exactly as in P1/P2.

Identifiers always use id_x format (per project spec).

Only global scope is implemented (100% option).

All input tokens are separated by spaces (per grading script).

9. Compilation and Runtime Requirements

C++17 standard

Works on the UMSL Delmar server environment

No external libraries required

10. Example Output
Valid program with no errors and no unused variables:
(no output)

Variable used before definition:
ERROR in P3: variable 'id_3' used before definition on line 6

Variable defined but never used:
WARNING in P3: variable 'id_4' defined on line 3 but never used

11. Acknowledgements

P1/P2 implementations from previous projects

Static semantics logic implemented directly from CS4280 project description

No external code sources used

ChatGPT used as a development assistant; all final code understood, integrated, and tested manually
