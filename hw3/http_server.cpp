#include <array>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <utility>

#include "funct.h"

#define PORT 7001 // 使用者將連線的 port
#define MAXLENGTH 3000


using namespace std;
using namespace boost::asio;

io_service global_io_service;

class HTTPSession : public enable_shared_from_this<HTTPSession> {
private:
    ip::tcp::socket _socket;
    array<char, MAXLENGTH> _data;
    Header header;

public:
    HTTPSession(ip::tcp::socket socket) : _socket(move(socket)) {}

    void start() { do_read(); }

private:
    void do_read() {
        auto self(shared_from_this());

        _socket.async_read_some(
                buffer(_data, MAXLENGTH),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        Header header;
                        string request;
                        request.append(_data.data(), _data.data()+length);


                        header.s_port = to_string(_socket.local_endpoint().port());
                        header.s_addr = _socket.local_endpoint().address().to_string();
                        header.r_port = to_string(_socket.remote_endpoint().port());
                        header.r_addr = _socket.remote_endpoint().address().to_string();

                        parse_req(request, &header);
                        set_envs(&header);


                        // 判斷 path 是否存在 (***.cgi)
                        // F_OK: 只檢查是否存在 / X_OK: 可執行 / R_OK : 可讀取 /
                        if (header.path.c_str() && access (header.path.c_str(), F_OK|X_OK) != -1) {		     // executable
                            OK (header.protocol);
                            execl (header.path.c_str(), header.path.c_str(), NULL);
                        } else if (access (header.path.c_str(), F_OK) != -1) {		     // 403
                            forbidden (header.protocol);
                        } else {						                                 // 404
                            not_found (header.protocol);
                            //header_print(header);
                        }
                    }
                });
    }
    void do_write(string const& message) {
        auto self(shared_from_this());
        _socket.async_send(
                buffer(message.c_str(), message.size()),
                [this, self](boost::system::error_code ec, std::size_t /* length */) {
                    if (!ec) {}
                });
    }

};

class HTTPServer {
private:
    ip::tcp::acceptor _acceptor;
    ip::tcp::socket _socket;

public:
    HTTPServer(short port)
            : _acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
              _socket(global_io_service) {
        do_accept();
    }

private:
    void do_accept() {
        _acceptor.async_accept(_socket, [this](boost::system::error_code ec) {
            if (!ec) {
                global_io_service.notify_fork(io_service::fork_prepare);

                int pid = fork();
                // child process
                if(pid == 0)
                {
                    // inform io_service fork finished (child)
                    global_io_service.notify_fork(io_service::fork_child);

                    // close acceptor
                    _acceptor.close();



                    dup2(_socket.native_handle(), STDIN_FILENO);
                    dup2(_socket.native_handle(), STDOUT_FILENO);
                    dup2(_socket.native_handle(), STDERR_FILENO);


                    // parse the request
                    make_shared<HTTPSession>(move(_socket))->start();


                } else {
                    // parent process
                    // inform io_service fork finished (parent)
                    global_io_service.notify_fork(io_service::fork_parent);

                    //cout << "----- " + _socket.remote_endpoint().address().to_string() +
                    //        ":"+ to_string(_socket.remote_endpoint().port()) + " pid:" + to_string(pid) + " -----\n";

                    _socket.close();
                    do_accept();
                }

            } else {
                cerr << "accept error:" << ec.message() << endl;
                do_accept();
            }
        });
    }
};



int main (int argc, char *argv[])
{
    signal(SIGCHLD, SIG_IGN);
    // set port
    short port = PORT;      // default PORT = 80
    if(argc == 2) {
        port = static_cast<short>(atoi(argv[1]));
    } else if (argc > 2) {
        cerr << "Usage:" << argv[0] << " [port]" << endl;
        return -1;
    }

    try {
        //EchoServer server(port);
        HTTPServer server(port);
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }

    /*
    string path = "/public_html";
    Header test_header;
    string test_request = "GET /test.cgi?a=b&c=d HTTP/1.1\n"
                          "Host: 127.0.0.1:7001\n"
                          "Connection: keep-alive\n"
                          "Upgrade-Insecure-Requests: 1\n"
                          "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.110 Safari/537.36\n"
                          "Accept-Encoding: gzip, deflate, br\n"
                          "Accept-Language: zh-TW,zh;q=0.9,en-US;q=0.8,en;q=0.7,zh-CN;q=0.6,ja;q=0.5";
    parse_req(test_request, &test_header);
    header_print(test_header);
    */

    return 0;
}
