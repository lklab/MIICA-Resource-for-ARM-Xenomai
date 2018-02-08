mkdir build
cd build

cmake -DCMAKE_MAKE_PROGRAM="E:/Work/Development/MIICA_Resource/Toolchaine/applied/bin/mingw-make/mingw32-make.exe" -DCMAKE_TOOLCHAIN_FILE=../toolchain.arm.cmake -G "MinGW Makefiles" ..
cmake --build .

@echo off
set /p str=completed
