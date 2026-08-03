@echo off
REM -DBUILD_INTERNAL=1 -nologo -WX -W4 -wd4100 -wd4189
mkdir ..\build
pushd ..\build
rm *
cl -std:c++20 -DBUILD_INTERNAL=1 -nologo -WX -W4 -wd4100 -wd4189 -Zi ..\code\win32_handmade.cpp ..\code\handmade.cpp ..\code\win32_wasapi.cpp User32.lib Gdi32.lib Ole32.lib
popd
