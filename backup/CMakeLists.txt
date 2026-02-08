cmake_minimum_required(VERSION 3.14)
project(aggregator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

add_executable(aggregator
    ../src/aggregator.cpp
    ../src/binance_connector.cpp
    ../src/bitget_connector.cpp
    ../src/main.cpp
    ../src/market_connector.cpp
    ../src/okx_connector.cpp
)

target_include_directories(aggregator PRIVATE 
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(aggregator
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)