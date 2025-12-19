Name: Curtis Been
Course: CS 4280
Project: P4 – Target Generation

This project completes the compiler by generating target assembly code
from the parse tree after successful static semantic analysis.

Invocation:
  compile [file]

Output:
  a.asm       (keyboard input)
  file.asm    (file input)

Notes:
- Global storage allocation is used
- Temporary variables begin with 't'
- All prior P1–P3 outputs are disabled
- Code generation is performed via parse tree traversal
