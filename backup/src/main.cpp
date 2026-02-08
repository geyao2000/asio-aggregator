#include <boost/asio/io_context.hpp>
#include "aggregator.h"

int main() {
    boost::asio::io_context ioc;
    aggregator agg(ioc);
    agg.start();
    ioc.run();
    return 0;
}
