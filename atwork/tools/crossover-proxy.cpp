/*
 *
 */

#define BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG

#include <config/yaml.h>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <protobuf_comm/client.h>
#include <utils/system/argparser.h>

#include <msgs/rci_pb_msgs/OrderInfo.pb.h>

using namespace protobuf_comm;
using namespace fawkes;

boost::asio::io_service io_service_;

static bool quit = false;

ProtobufStreamClient*	atwork_client_;
ProtobufStreamClient*	rcll_client_;
std::string		atwork_host_;
int			atwork_port_;
std::string		rcll_host_;
int			rcll_port_;
std::shared_ptr<rci_pb_msgs::Order> atwork_order_;
std::shared_ptr<rci_pb_msgs::Order> rcll_order_;

boost::mutex mutex;


void signal_handler(const boost::system::error_code& error, int signum)
{
  if (!error) {
    quit = true;
    io_service_.stop();
  }
}

void handle_atwork_msg(uint16_t comp_id, uint16_t msg_type,
       std::shared_ptr<google::protobuf::Message> msg)
{
  boost::mutex::scoped_lock lock(mutex);

  if (std::dynamic_pointer_cast<rci_pb_msgs::Order>(msg)) {
    atwork_order_ = std::dynamic_pointer_cast<rci_pb_msgs::Order>(msg);
    
    std::cout << "handle_atwork_msg" << std::endl;
    if (!rcll_client_->connected()) return;
    std::cout << "rcll is connected" << std::endl;
    rci_pb_msgs::Order order;
    order.set_id(atwork_order_->id());
    order.set_cap_color(atwork_order_->cap_color());
    order.set_quantity_requested(atwork_order_->quantity_requested());
    order.set_quantity_delivered(atwork_order_->quantity_delivered());
    rcll_client_->send(order);
  }
}

void handle_atwork_disconnect(const boost::system::error_code &error)
{
  usleep(100000);
  atwork_client_->async_connect(atwork_host_.c_str(), atwork_port_);
}

void handle_rcll_msg(uint16_t component_id, uint16_t msg_type,
       std::shared_ptr<google::protobuf::Message> msg)
{
  boost::mutex::scoped_lock lock(mutex);

  if (!atwork_order_) return;

  if (std::dynamic_pointer_cast<rci_pb_msgs::Order>(msg)) {
    rcll_order_ = std::dynamic_pointer_cast<rci_pb_msgs::Order>(msg);

    if (rcll_order_->id() == atwork_order_->id()) {
      if (rcll_order_->quantity_requested() <= rcll_order_->quantity_delivered()) {
        std::cout << "ORDER IS DONE for RCLL" << std::endl;
        quit = true;
        io_service_.stop();
      }
    }
  } else {
    std::cout << "Can't decode msg" << std::endl;
  }
}

void handle_rcll_disconnect(const boost::system::error_code &error)
{
  usleep(100000);
  rcll_client_->async_connect(rcll_host_.c_str(), rcll_port_);
}

int main(int argc, char **argv)
{
  std::cout << "load config" << std::endl;
  llsfrb::YamlConfiguration config(CONFDIR);
  config.load("config.yaml");

  printf("get atwork info");
  atwork_host_ = config.get_string("/llsfrb/shell/refbox-host");
  atwork_port_ = config.get_uint("/llsfrb/shell/refbox-port");

  printf("get rcll info");
  rcll_host_ = config.get_string("/llsfrb/crossover/rcll-host");
  rcll_port_ = config.get_uint("/llsfrb/crossover/rcll-port");

  std::cout << "new stream clients" << std::endl;
  atwork_client_ = new ProtobufStreamClient();
  rcll_client_ = new ProtobufStreamClient();

  MessageRegister& atwork_msgr_ = atwork_client_->message_register();
  MessageRegister& rcll_msgr_  = rcll_client_->message_register();

  atwork_msgr_.add_message_type<rci_pb_msgs::Order>();
  rcll_msgr_.add_message_type<rci_pb_msgs::Order>();
  std::cout << "registered msgs" << std::endl;

  rcll_client_->signal_received().connect(handle_rcll_msg);
  rcll_client_->signal_disconnected().connect(handle_rcll_disconnect);
  rcll_client_->async_connect(rcll_host_.c_str(), rcll_port_);
  std::cout << "connect rcll" << std::endl;

  atwork_client_->signal_received().connect(handle_atwork_msg);
  atwork_client_->signal_disconnected().connect(handle_atwork_disconnect);
  atwork_client_->async_connect(atwork_host_.c_str(), atwork_port_);
  std::cout << "connect atwork" << std::endl;

  std::cout << "signal set" << std::endl;
  // Construct a signal set registered for process termination.
  boost::asio::signal_set signals(io_service_, SIGINT, SIGTERM);

  // Start an asynchronous wait for one of the signals to occur.
  signals.async_wait(signal_handler);

  do {
    std::cout << "do-while" << std::endl;
    io_service_.run();
    io_service_.reset();
  } while (! quit);

  delete atwork_client_;
  delete rcll_client_;

  // Delete all global objects allocated by libprotobuf
  google::protobuf::ShutdownProtobufLibrary();
}
