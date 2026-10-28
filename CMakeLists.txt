# 1. 指定 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.10)

# 2. 声明项目名称和使用的语言
project(CppWebServer CXX)

# 3. 设置 C++ 标准为 C++17 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. 自动收集当前目录下所有的 .cpp 源文件
file(GLOB SOURCES "*.cpp")

# 5. 寻找系统里的多线程库 (对应 -pthread)
find_package(Threads REQUIRED)

# 6. 生成可执行文件，名字叫 server，由上面的 SOURCES 编译而来
add_executable(server ${SOURCES})

# 7. 把多线程库链接到我们的 server 上
target_link_libraries(server Threads::Threads)