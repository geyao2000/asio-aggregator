---

    +------------------+
    |   Binance CEX    |
    +------------------+
             |
    +------------------+
    |     OKX CEX      |
    +------------------+        +--------------------+
             |                  |                    |
    +------------------+ -----> |   Aggregator       | -----> gRPC -----> Clients
    |    Bybit CEX     |        |   (Consolidator)   |                   (BBO, Bands)
    +------------------+        |                    |
             |                  +--------------------+
    +------------------+
    |   Additional CEX |
    +------------------+


## Overview

This project aggregates depth updates from major crypto exchanges and provides a single gRPC service for clients to subscribe to the merged orderbook (bids/asks). It supports:

- Real-time incremental + snapshot updates
- Standardized price/quantity precision
- Multi-client support (BBO, volume bands, price bands)
- Fully containerized with Docker and Docker Compose

## Architecture

- **aggregator**: gRPC server that subscribes to WebSocket feeds from exchanges, merges orderbooks, and streams updates on port 50051.
- **client-bbo**: Best Bid/Offer client — subscribes and prints top bid/ask.
- **client-volume-bands**: Volume bands client — monitors volume in price ranges.
- **client-price-bands**: Price bands client — monitors price movements in ranges.

	Each component runs in its own Docker container. The system uses docker-compose for orchestration on a single host.

## Tech Stack

- C++17
- gRPC v1.62.0 + Protobuf v3.25.3
- Abseil LTS 20230802.1
- Boost 1.74+ (system, thread)
- nlohmann/json v3.11.3 (header-only)
- WebSocket: Boost.Beast
- Base OS: Ubuntu 22.04

## Prerequisites

- Docker & Docker Compose installed

	sudo apt update && \
	sudo apt install docker.io && \
	sudo apt install docker-compose-v2

- Git

	git clone https://github.com/geyao2000/asio-aggregator.git

## Build Instructions

1. Build the base image (pre-compiled heavy dependencies — only needed once or when deps change)

    sudo docker build --no-cache -f docker/Dockerfile.base -t asio-aggregator-base:latest .

or, download from docker hub:

	sudo docker pull geyao2000/asio-aggregator-base:latest

rename:

	sudo docker tag geyao2000/asio-aggregator-base:latest asio-aggregator-base:latest

2. Use docker compose to build everything and start network at once 

	cd ~/asio-aggregator && \
	sudo docker compose up -d --build

or if you want to build/run individually
3. Build aggregator server image

	sudo docker build -f docker/Dockerfile.aggregator -t asio-aggregator-service:latest .

4. Build the three clients imgages

	sudo docker build -f docker/Dockerfile.client_bbo -t img_client_bbo:latest .
	sudo docker build -f docker/Dockerfile.client_price_bands -t img_client_price_bands:latest .
	sudo docker build -f docker/Dockerfile.client_volume_bands -t img_volume_price_bands:latest .


5. Create Network

	sudo docker network create my-trading-net
	
## Run the System

	Option 1: (Recommended) Using docker compose 
	
	sudo docker compose up -d
	
	# Starts aggregator server + all three clients.
	# Clients automatically connect to aggregator:50051.
	
	# View status:
	
	sudo docker compose ps
	
	Option 2: Manual runs
	
	# Start aggregator server
	
	sudo docker run -d --name aggregator --network my-trading-net -p 50051:50051 asio-aggregator-service:latest
	
	# Start clients (connect to aggregator)
	
	sudo docker run -d --name client_bbo --network my-trading-net img_client_bbo:latest
	sudo docker run -d --name client_price_bands --network my-trading-net img_client_price_bands:latest
	sudo docker run -d --name client_volume_bands --network my-trading-net img_client_volume_bands:latest
	
## Check Status

	1. Check server is running
	
		sudo docker logs -f aggregator
		# Look for "Aggregator gRPC server running on port 50051"

	2. Test subscription (requires grpcurl)
	
		grpcurl -plaintext -d '{}' localhost:50051 aggregator.AggregatorService/SubscribeBook
		#You should see real-time BookUpdate messages (timestamp_ms + bids/asks).

	3. Check client logs
	
		sudo docker logs -f client-bbo

## Stop 

	sudo docker compose down

## Technical Decisions

	1. OrderBook Data Structure in std::map instead of std::unordered_map

		Efficiency in calculation: 
			Red-Black Tree, automatically sorts keys (prices). consolidated_bids_ uses std::greater<double> to keep the highest bid at begin(), consolidated_asks_ uses the default ascending order to keep the lowest ask at begin(). More efficient calculation with price bands and volume bands. In contrast, an unordered_map would require a full O(N(log N)) sort for every update, which is prohibitive in low-latency systems.
		
		Memory Allocation Overhead: 
			As a node-based container, std::map triggers a heap allocation (new) for every new price level, potentially leading to memory fragmentation and cache misses.
		
	2. Multi-threaded vs Boost.Beast/Asio
		
		Apply Beast/Asio. Multiple CEX connector compete for consolidated_mutex_. gRPC streaming threads(BBO, Volume/Price Bands) lock mutex to read; under high market volatility, mutex contention becomes a significant bottleneck. Beast has: Asynchorous architecture, event-driven design, non-blocking model. 
			
	3. Data process vs network load
	
		Aggregator only consolidate CEX's data, pushing stream to clients with no storing or processing. This simplicity makes the ultra fast speed and the architecture easier to maintain. Also it reduced resource overhead.
		The high network load does reduce upper limit of the connectivity. However, here we have only  4 CEX and 3 clients. When the number goes up we will need to balance calculation and the bandwidth.
	
	4. Multi-stage builds 
		
		Heavy compilation in builder stage, runtime image is minimal (~200MB). Pre-built base image (aggregator-base) — Contains compiled gRPC, Protobuf, Abseil, Boost etc. → fast incremental builds. 
		
		Independent containers per service — Fault isolation, independent scaling/restart, clear logs/monitoring.
		
	5. docker-compose 
	
		Single command to start everything, automatic dependency ordering (depends_on).
	
	6. Proto files generated at build time 
	
		Keeps source tree clean (generated in build/generated).
	
	7. Static linking preference for heavy deps 
	
		Reduces runtime dependencies (though dynamic linking used here for compatibility).
	
	8. Manual json.hpp download 
	
		Avoids FetchContent network issues in Docker.

Dependencies

	gRPC v1.62.0
	
	Protobuf v3.25.3
	
	Abseil LTS 20230802.1
	
	Boost 1.74+
	
	nlohmann/json v3.11.3
	
	Ubuntu 22.04 base
	

	
