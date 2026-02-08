cmake_minimum_required(VERSION 3.16)
project(asio_aggregator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

include_directories(${Boost_INCLUDE_DIRS} include)

add_executable(asio_aggregator
    src/main.cpp
    src/connector.cpp
    src/binance_connector.cpp
)

target_link_libraries(asio_aggregator
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)
