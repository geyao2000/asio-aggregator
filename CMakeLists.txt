cmake_minimum_required(VERSION 3.10)
project(asio_aggregator CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 查找依赖库
# 查找 Boost (需要 System 和 Thread 组件)
find_package(Boost REQUIRED COMPONENTS system thread)
# 查找 OpenSSL (Beast SSL 必须)
find_package(OpenSSL REQUIRED)
# 查找 Threads (POSIX 线程)
find_package(Threads REQUIRED)

# 2. 包含头文件目录
include_directories(include)

# 3. 指定源文件
# 假设你的文件名分别为:
# src/main.cpp
# src/connector.cpp
# src/aggregator.cpp
# src/binance_connector.cpp
set(SOURCES 
    src/main.cpp
    src/connector.cpp
    # src/Aggregator.cpp
    src/binance_connector.cpp
)

# 4. 创建可执行文件
add_executable(aggregator_test ${SOURCES})

# 5. 链接库
target_link_libraries(aggregator_test
    PRIVATE
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# 如果你使用的是系统安装的 nlohmann_json，取消下面行的注释
# find_package(nlohmann_json REQUIRED)
# target_link_libraries(aggregator_test PRIVATE nlohmann_json::nlohmann_json)
# 如果是 header-only 且在 include 目录下则无需额外操作
