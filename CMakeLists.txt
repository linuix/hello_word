# 指定 cmake 版本，跟安装的版本保持一致
cmake_minimum_required(VERSION 3.2.1)
 
# 指定 c++ 标准
set(CMAKE_CXX_STANDARD 11)
add_compile_options(-std=c++11)
 
# specify the cross compiler
# SET(CMAKE_C_COMPILER   /opt/hisi-linux/x86-arm/arm-himix100-linux/bin/arm-himix100-linux-gcc)
# SET(CMAKE_CXX_COMPILER /opt/hisi-linux/x86-arm/arm-himix100-linux/bin/arm-himix100-linux-g++)

# 输出编译信息
message("-------- Start compile ------------------")
 
# 设置源码根路径，即 cpp 目录
set(SRC_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
message("SRC_ROOT_PATH: " ${SRC_ROOT_PATH})
 
# 定义要编译的文件列表变量
file(GLOB CPP_FILES_IN_UTILS "${SRC_ROOT_PATH}./*.cpp")
 
aux_source_directory("${SRC_ROOT_PATH}" SRC_PATH)

# set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_BUILD_TYPE "Release")

# 设置可执行输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${SRC_ROOT_PATH}/bin/Debug)    
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${SRC_ROOT_PATH}/bin/Release) 


# 设置要编译的源文件
set(
        SOURCE_FILES
        ${CPP_FILES_IN_UTILS}
        ${SRC_PATH}
        ./hello.cpp;
        
)
 
# include 头文件目录
include_directories(${SRC_ROOT_PATH}./include/)
link_directories(xiaomi4c-lib)
 
# 编译成可执行文件，如果编译动态库可以用：target_link_libraries
add_executable(recordService ${SOURCE_FILES})

# find_library(cutils.so)
# find_library(record,"/data/local")


find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# target_compile_options(hello PUBLIC "-Wl,-rpath=/data/local")
target_link_libraries( # Specifies the target library.
                        recordService
                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib}
                       tinyalsa
                       cutils
                       utils
                #        -Wl,-rpath .
                       )
                       

