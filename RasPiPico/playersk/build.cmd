@echo off
set BNAME=playersk
cls
del %BNAME%.lst 
.\AILZ80ASM -i %BNAME%.z80 -f -bin %BNAME%.com -lst -lm full -ts 4
rem .\AILZ80ASM -i %BNAME%.z80 -f -bin %BNAME%.com