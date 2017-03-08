#include <memory>
#include <thread>
#include <chrono>

#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include "Listener.h"
#include "NetworkThread.h"
#include "Scoket.h"

using namespace Origin;
using namespace boost::asio::ip;
