@echo off
set COMPILER_FLAGS=-std:c++20 -DBUILD_INTERNAL=1 -nologo -WX -W4 -wd4100 -wd4189 -Zi
set LINKER_FLAGS=User32.lib Gdi32.lib Ole32.lib Avrt.lib

mkdir ..\build
pushd ..\build
del *
cl %COMPILER_FLAGS% ..\code\handmade.cpp /LD
cl %COMPILER_FLAGS% ..\code\win32_handmade.cpp ..\code\win32_wasapi.cpp %LINKER_FLAGS%
popd
