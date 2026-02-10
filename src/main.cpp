#include <boost/asio/io_context.hpp>
#include "Aggregator.h"

int main(int argc, char** argv) {
    std::string config_file = "connectors.json";
    if (argc > 1) {
        config_file = argv[1];
    }

    boost::asio::io_context ioc;
    Aggregator agg(ioc);
    agg.start(config_file);
    ioc.run();
    return 0;
}
