mkdir build
cd build

cmake -DCMAKE_MAKE_PROGRAM="E:/Work/Development/UTOPIIA_Resource/Toolchaine/mingw-make/mingw32-make.exe" -DCMAKE_TOOLCHAIN_FILE=../toolchain.arm.cmake -G "MinGW Makefiles" ..
cmake --build .

@echo off
set /p str=completed
