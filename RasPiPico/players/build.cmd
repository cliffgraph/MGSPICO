cls
@echo off
set BNAME=players
echo --- for MGSDRV %BNAME% ----------------------------------
del %BNAME%.lst 
.\AILZ80ASM -i %BNAME%.z80 -f -bin %BNAME%.com -lst -lm full -ts 4

set BNAME=playersk
echo --- for KINROU5(MuSICA) %BNAME% -------------------------
del %BNAME%.lst 
.\AILZ80ASM -i %BNAME%.z80 -f -bin %BNAME%.com -lst -lm full -ts 4

pause