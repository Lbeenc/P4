Curtis Been
CS4280 - Project P4 (Target Generation and Storage Allocation)

Build:
  make

Invocation:
  ./compile            (reads program from keyboard/stdin, outputs a.asm)
  ./compile <filebase> (reads <filebase>.fs25s2, outputs <filebase>.asm)

Notes:
- Project includes P1 scanner, P2 parser (parse tree), P3 static semantics, and P4 code generation.
- When input is correct, P4 should not print extra debug output.
- Static semantics behavior:
    ERROR in P3: ...  (printed to stdout, then exit)
    WARNING in P3: ... (printed to stdout, continue)

Output:
- a.asm when compiling from stdin
- <filebase>.asm when compiling from a file argument
