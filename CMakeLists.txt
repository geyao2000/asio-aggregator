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
  # src/bitget_connector.cpp
  src/bybit_connector.cpp
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

add_executable(client_bbo
  src/client_bbo.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.grpc.pb.cc
)
target_link_libraries(client_bbo gRPC::grpc++ protobuf::libprotobuf)

add_executable(client_volume_bands
  src/client_volume_bands.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.grpc.pb.cc
)
target_link_libraries(client_volume_bands gRPC::grpc++ protobuf::libprotobuf)

add_executable(client_price_bands
  src/client_price_bands.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/aggregator.grpc.pb.cc
)
target_link_libraries(client_price_bands gRPC::grpc++ protobuf::libprotobuf)

# add_executable(tests src/tests.cpp 
    # src/binance_connector.cpp src/okx_connector.cpp src/bitget_connector.cpp
    # # 其他依赖源文件
    # ${GENERATED_PROTO_FILES}
# )

# target_link_libraries(tests PRIVATE Catch2::Catch2WithMain 
    # Boost::system nlohmann_json::nlohmann_json
    # # 如果需要 gRPC 等
# )

# target_include_directories(tests PRIVATE
    # ${CMAKE_CURRENT_SOURCE_DIR}/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/src
    # ${CMAKE_CURRENT_BINARY_DIR}
# )