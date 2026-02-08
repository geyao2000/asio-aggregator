cmake_minimum_required(VERSION 3.16)
project(aggregator_project)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
# find_package(Catch2 REQUIRED) # For tests

# Generate gRPC and Protobuf code
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/aggregator.pb.cc" "${CMAKE_CURRENT_BINARY_DIR}/aggregator.pb.h"
         "${CMAKE_CURRENT_BINARY_DIR}/aggregator.grpc.pb.cc" "${CMAKE_CURRENT_BINARY_DIR}/aggregator.grpc.pb.h"
  COMMAND protobuf::protoc
  ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}" --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
       -I "${CMAKE_SOURCE_DIR}/proto"
       --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
       "${CMAKE_SOURCE_DIR}/proto/aggregator.proto"
  DEPENDS "${CMAKE_SOURCE_DIR}/proto/aggregator.proto"
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}" include)

add_executable(aggregator
  src/main.cpp
  src/Aggregator.cpp
  src/market_connector.cpp
  src/binance_connector.cpp
  src/okx_connector.cpp
  src/bitget_connector.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.grpc.pb.cc
)

target_link_libraries(aggregator
  Boost::system
  Boost::thread
  OpenSSL::SSL
  OpenSSL::Crypto
  gRPC::grpc++
  protobuf::libprotobuf
  nlohmann_json::nlohmann_json
)

add_executable(bbo_client
  src/bbo_client.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.grpc.pb.cc
)
target_link_libraries(bbo_client gRPC::grpc++ protobuf::libprotobuf)

# Similar for volume_bands_client and price_bands_client

# add_executable(tests src/tests.cpp ... ) 
# target_link_libraries(tests Catch2::Catch2 ... )