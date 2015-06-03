@call c:\watcom\setvars.bat >> NUL:
wasm -q solvbeasm.asm
wpp -zq -ms SolVBE.cpp 
rem -ox -zq -6 -j -ms -d0 
wcl -zq -ms SolVBE.obj solvbeasm.obj
@echo.
@echo.
pause


