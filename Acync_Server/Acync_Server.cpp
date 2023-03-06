#define _CRT_SECURE_NO_WARNINGS
#include <ctime>

#include <stdio.h>
#include <fstream>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>

#include <boost/log/sinks/unlocked_frontend.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

using namespace boost::asio;
using namespace boost::posix_time;



io_service service;
#define BOOST_LOG_NO_THREADS
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

inline std::string getCurrentDateTime(std::string s) {
    time_t now = time(0);
    struct tm  tstruct;
    char  buf[80];
    tstruct = *localtime(&now);
    if (s == "now")
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    else if (s == "date")
        strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
    return std::string(buf);
};
inline void Logger(std::string logMsg) {
    std::string filePath = getCurrentDateTime("date") + ".txt";
    std::string now = getCurrentDateTime("now");
    std::ofstream ofs(filePath.c_str(), std::ios_base::out | std::ios_base::app);
    ofs << "[" << now << "]" << '\t' << logMsg << '\n';
    ofs.close();
}

#include <iostream>

class talk_to_client : public boost::enable_shared_from_this<talk_to_client>, boost::noncopyable {
    typedef talk_to_client self_type;
    talk_to_client() : sock_(service), started_(false) {}
public:
    typedef boost::system::error_code error_code;
    typedef boost::shared_ptr<talk_to_client> ptr;

    void start() {
        started_ = true;
        Logger("Getting json from client...");
        BOOST_LOG_TRIVIAL(info) << "Getting json from client...";
        //std::cout << "Getting json from client..." << std::endl;
        do_read();
    }
    static ptr new_() {
        ptr new_(new talk_to_client);
        return new_;
    }
    void stop() {
        if (!started_) return;
        started_ = false;
        Logger("Connection to client closed");
        BOOST_LOG_TRIVIAL(info) << "Connection to client closed";
        //std::cout << "Connection to client closed" << std::endl;
        sock_.close();
    }
    ip::tcp::socket& sock() { return sock_; }
private:
    void on_read(const error_code& err, size_t bytes) {
        if (!err) {
            std::string msg(read_buffer_, bytes);
            std::string s = msg;

            if (!s.empty()) {
                s.resize(s.size() - 1);
            }

            std::stringstream jsonEncodedData(s);
            boost::property_tree::ptree rootHive;
            boost::property_tree::read_json(jsonEncodedData, rootHive);

            std::string myString = "{}";
            std::stringstream myJsonEncodedData(myString);
            boost::property_tree::ptree rootServer;

            Logger("Read json from client : " + s);
            BOOST_LOG_TRIVIAL(info) << "Read json from client : " << s;
            //std::cout << "Read json from client:" << s << std::endl;
            std::cout << "Client sent equation: " << rootHive.get<double>("equation.X")
                << rootHive.get<std::string>("equation.operator")
                << rootHive.get<double>("equation.Y") << std::endl;

            if (rootHive.get<std::string>("equation.operator") == "+") {
                std::cout << "Answer: " << rootHive.get<double>("equation.X") +
                    rootHive.get<double>("equation.Y") << std::endl;
                rootServer.put("answer", rootHive.get<double>("equation.X") +
                    rootHive.get<double>("equation.Y"));
            }
            else if (rootHive.get<std::string>("equation.operator") == "-") {
                std::cout << "Answer: " << rootHive.get<double>("equation.X") -
                    rootHive.get<double>("equation.Y") << std::endl;
                rootServer.put("answer", rootHive.get<double>("equation.X") -
                    rootHive.get<double>("equation.Y"));
            }
            else if (rootHive.get<std::string>("equation.operator") == "*") {
                std::cout << "Answer: " << rootHive.get<double>("equation.X") *
                    rootHive.get<double>("equation.Y") << std::endl;
                rootServer.put("answer", rootHive.get<double>("equation.X") *
                    rootHive.get<double>("equation.Y"));
            }
            else if (rootHive.get<std::string>("equation.operator") == "/") {
                if (rootHive.get<double>("equation.X") != 0) {
                    std::cout << "Answer: " << rootHive.get<double>("equation.X") /
                        rootHive.get<double>("equation.Y") << std::endl;
                    rootServer.put("answer", rootHive.get<double>("equation.X") /
                        rootHive.get<double>("equation.Y"));
                }
                else std::cout << "Answer: Y can't be zerro" << std::endl;
            }
            else std::cout << "Answer: Operator error" << std::endl;

            boost::property_tree::write_json(myJsonEncodedData, rootServer);

            boost::json::error_code errorCode;
            //boost::json::value jsonValue = boost::json::parse(myJsonEncodedData, errorCode);
            auto jsonValue = boost::json::parse(myJsonEncodedData, errorCode);
            std::ostringstream osstr;
            osstr << jsonValue;
            std::string zapros = osstr.str();
            do_write(zapros + "\n");
        }
        //else std::cout << err << std::endl;
        stop();
    }

    void on_write(const error_code& err, size_t bytes) {
        do_read();
    }
    void do_read() {
        async_read(sock_, buffer(read_buffer_),
            MEM_FN2(read_complete, _1, _2), MEM_FN2(on_read, _1, _2));
    }
    void do_write(const std::string& msg) {
        Logger("Loading json to buffer: "  + msg);
        BOOST_LOG_TRIVIAL(info) << "Loading json to buffer: " << msg;
        //std::cout << "Loading json to buffer: " << msg << std::endl;
        std::copy(msg.begin(), msg.end(), write_buffer_);
        Logger("Sending json to client...");
        BOOST_LOG_TRIVIAL(info) << "Sending json to client...";
        //std::cout << "Sending json to client..." << std::endl;
        sock_.async_write_some(buffer(write_buffer_, msg.size()),
            MEM_FN2(on_write, _1, _2));
        Logger("Sending complete!");
        BOOST_LOG_TRIVIAL(info) << "Sending complete!";
        //std::cout << "Sending complete!" << std::endl;
    }
    size_t read_complete(const boost::system::error_code& err, size_t bytes) {
        if (err) return 0;
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
        // we read one-by-one until we get to enter, no buffering
        return found ? 0 : 1;
    }
private:
    ip::tcp::socket sock_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
};

ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 8001));

void handle_accept(talk_to_client::ptr client, const boost::system::error_code& err) {
    Logger("Client connected to server.");
    BOOST_LOG_TRIVIAL(info) << "Client connected to server.";
    client->start();
    talk_to_client::ptr new_client = talk_to_client::new_();
    acceptor.async_accept(new_client->sock(), boost::bind(handle_accept, new_client, _1));
}


int main(int, char* []) {

    //boost::filesystem::path::imbue(std::locale("C"));
    //boost::log::add_file_log("test.log");

    talk_to_client::ptr client = talk_to_client::new_();
    acceptor.async_accept(client->sock(), boost::bind(handle_accept, client, _1));
    Logger("Server starting work...");
    BOOST_LOG_TRIVIAL(info) << "Server starting work...";
    service.run();
}