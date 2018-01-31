cmake_minimum_required(VERSION 3.5)

add_definitions(-D_GNU_SOURCE -D_REENTRANT -Wall -pipe -D__XENO__)

link_libraries(native xenomai pthread ethercat_rtdm rtdm)

add_library(os STATIC os.c)
add_library(igh STATIC igh_app.c cia402.c igh.c)
