CS4280 P4 - Target Generation and Storage Allocation
Name: Curtis Been
SSO: cjb2m8

Build:
  make

Run (file input, implicit extension supported):
  ./compile test
  (tries "test" then "test.fs25s2")

Run (explicit extension also works):
  ./compile test.fs25s2

Keyboard input (produces a.asm):
  ./compile

Output:
  - If keyboard input: a.asm
  - If file input: <file>.asm (file base name)

Notes:
  - This project integrates P1 (scanner), P2 (parser), P3 (static semantics), and P4 (code generation).
  - P3 errors/warnings are handled before code generation.
