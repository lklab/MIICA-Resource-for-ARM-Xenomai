SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR arm)

SET(COMPILER_ROOT "E:/Work/Development/MIICA_Resource/Toolchaine/applied/mingw/bin/arm-linux-gnueabihf-")

SET(CMAKE_C_COMPILER ${COMPILER_ROOT}gcc.exe)
SET(CMAKE_CXX_COMPILER E:/Work/Development/MIICA_Package/MIICA/Resources/build/ARM_Linux/gcc-linaro-7.1.1-2017.08-i686-mingw32_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++.exe)
SET(CMAKE_LINKER ${COMPILER_ROOT}ld.exe)
SET(CMAKE_NM ${COMPILER_ROOT}nm.exe)
SET(CMAKE_OBJCOPY ${COMPILER_ROOT}objcopy.exe)
SET(CMAKE_OBJDUMP ${COMPILER_ROOT}objdump.exe)
SET(CMAKE_RANLIB ${COMPILER_ROOT}ranlib.exe)
