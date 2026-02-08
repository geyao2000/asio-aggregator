FROM ubuntu:22.04 as builder

RUN apt-get update && apt-get install -y build-essential autoconf libtool pkg-config cmake git libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler nlohmann-json3-dev libcatch2-dev

RUN git clone --recurse-submodules -b v1.62.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc && \
    cd grpc && mkdir -p cmake/build && cd cmake/build && \
    cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release ../.. && \
    make -j$(nproc) && make install

COPY . /app

RUN cd /app && mkdir build && cd build && cmake .. && make -j$(nproc)

FROM ubuntu:22.04

COPY --from=builder /app/build/aggregator /usr/local/bin/aggregator
COPY --from=builder /app/build/bbo_client /usr/local/bin/bbo_client
COPY --from=builder /app/build/volume_bands_client /usr/local/bin/volume_bands_client
COPY --from=builder /app/build/price_bands_client /usr/local/bin/price_bands_client

RUN apt-get update && apt-get install -y libboost-system1.74.0 libboost-thread1.74.0 libssl3

CMD ["aggregator"]