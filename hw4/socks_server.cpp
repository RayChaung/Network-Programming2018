#include <array>
#include <fstream>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <utility>
#include "socks4.h"


#define PORT 1088 // 使用者將連線的 port
//#define MAXLENGTH 2048
#define MAXLENGTH 8196

using namespace std;
using namespace boost::asio;

io_service global_io_service;
const string config_file = "socks.conf";
int Firewall (string ip, string mode);



class SOCKSSession : public enable_shared_from_this<SOCKSSession> {
private:
    ip::tcp::socket _socket;
    ip::tcp::socket tcp_socket;
    ip::tcp::acceptor bind_acceptor;
    array<char, MAXLENGTH> in_buf_;
    array<char, MAXLENGTH> out_buf_;
    socks4::request socks_request;
    ip::tcp::resolver resolv;
    enum read_type
    {
        read_client = 0x01,
        read_HTTP = 0x02,
        read_both = 0x03
    };

public:
    SOCKSSession(ip::tcp::socket socket) : _socket(move(socket)),
                                           resolv(global_io_service),
                                           bind_acceptor(global_io_service),
                                           tcp_socket(global_io_service)
    {}

    void start() {
        do_parse_request();
         }

private:
    void do_parse_request() {
        auto self(shared_from_this());

        _socket.async_read_some(
                socks_request.buffers(),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        cout << "<S_IP>\t\t:" << _socket.remote_endpoint().address() << endl;
                        cout << "<S_PORT>\t:" << _socket.remote_endpoint().port()    << endl;
                        cout << "<D_IP>\t\t:" << socks_request.endpoint().address()  << endl;
                        cout << "<D_PORT>\t:" << socks_request.endpoint().port()     << endl;
                        cout << "<Command>\t:"<< socks_request.s_command()           << endl;
                        //cout << "<User_ID>\t:"<< socks_request.user_id()             << endl;

                        // check if D_IP is illegal
                        if (Firewall(socks_request.endpoint().address().to_string()
                                , socks_request.short_command()) == -1) {
                            cout << "<Reply>\t\t:"<< "Reject" << endl;
                            cout << "----------------------------------" << endl << flush;
                            socks4::reply socks_reply(socks4::reply::request_failed, socks_request.endpoint());
                            do_send_reply(socks_reply);
                            _socket.close();
                        } else {
                            cout << "<Reply>\t\t:" << "Accept" << endl;
                            cout << "----------------------------------" << endl << flush;

                            // do command (switch by mode)
                            switch (socks_request.command()) {
                                case socks4::request::command_type::connect: {
                                    // 建連線
                                    ip::tcp::endpoint d_endpoint = socks_request.endpoint();
                                    ip::tcp::resolver::query q(d_endpoint.address().to_string(),
                                                               to_string(d_endpoint.port()));
                                    do_resolve(q);

                                }
                                break;
                                case socks4::request::command_type::bind: {

                                    unsigned short port(0);     // choose unuse port
                                    ip::tcp::endpoint bind_endPoint(ip::tcp::endpoint(ip::tcp::v4(), port));
                                    bind_acceptor.open(bind_endPoint.protocol());
                                    bind_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
                                    bind_acceptor.bind(bind_endPoint);
                                    bind_acceptor.listen();
                                    // send first reply
                                    socks4::reply socks_reply(socks4::reply::request_granted, bind_acceptor.local_endpoint());
                                    write(_socket, socks_reply.buffers());
                                    //bind_acceptor.accept(tcp_socket);
                                    //write(_socket, socks_reply.buffers());
                                    //do_read(read_both);
                                    do_accept(socks_reply);
                                    /*cout<< "endpoint:" << socks_request.endpoint().address().to_string() << "/"
                                        << socks_request.endpoint().port() << endl;
                                    cout<< "local_endpoint:" << bind_acceptor.local_endpoint().address().to_string() << "/"
                                        << bind_acceptor.local_endpoint().port() << endl;*/
                                }
                                break;
                            }
                        }
                    }
                });
    }
    void do_accept(socks4::reply socks_reply) {
        auto self(shared_from_this());
        bind_acceptor.async_accept(tcp_socket, [this, self, socks_reply](boost::system::error_code ec) {
            if (!ec) {
                do_send_reply(socks_reply);
            }
            else {
                _socket.close();
                cerr << "accept error.\n";
            }
        });
    }
    void do_resolve(ip::tcp::resolver::query q) {
        auto self(shared_from_this());      // make sure that ClientSession object outlives the asynchronous operation
        resolv.async_resolve(q,
                             [this, self](boost::system::error_code ec, ip::tcp::resolver::iterator it) {
                                 if (!ec) {
                                     //cout << "resolve sucess" << endl << flush;
                                     do_connect(it);
                                 }
                                 else cerr << "resolve error" << endl;
                             });
    }
    void do_connect(ip::tcp::resolver::iterator it) {
        auto self(shared_from_this());
        tcp_socket.async_connect(*it,
                                 [this, self](boost::system::error_code ec) {
                                     if (!ec) {
                                         //cout << "connect sucess" << endl << flush;
                                         socks4::reply socks_reply(socks4::reply::request_granted, socks_request.endpoint());
                                         do_send_reply(socks_reply);
                                     }
                                     else {
                                         socks4::reply socks_reply(socks4::reply::request_failed, socks_request.endpoint());
                                         do_send_reply(socks_reply);
                                         cerr << "connect error" << endl;
                                     }
                                 });
    }
    void do_send_reply(socks4::reply socks_reply) {
        auto self(shared_from_this());
        //socks4::reply socks_reply(status, socks_request.endpoint());
        _socket.async_send(
                socks_reply.buffers(),
                [this, self, socks_reply](boost::system::error_code ec, size_t /* length */) {
                    if (!ec) {
                        //cout << "send_reply sucess" << endl << flush;
                        // most be listening from both side
                        if(socks_reply.status() == socks4::reply::request_granted)
                            do_read(read_both);
                        else {
                            _socket.close(); tcp_socket.close();
                        }
                    }
                    else cerr << "send_reply error" << endl;
                });
    }
    void do_read(int direction)
    {
        auto self(shared_from_this());

        // We must divide reads by direction to not permit second read call on the same socket.
        if (direction == read_client || direction == read_both)
            _socket.async_read_some(
                    buffer(in_buf_),
                    [this, self](boost::system::error_code ec, size_t length) {
                        if (!ec) {
                            string request;
                            request.append(in_buf_.data(), in_buf_.data()+length);
                            //cout << "request:" << request << endl;
                            do_write(1, length);
                        } else //if (ec != boost::asio::error::eof)
                        {
                            // Most probably client closed socket. Let's close both sockets and exit session.
                            _socket.close(); tcp_socket.close();
                        }

                    });

        if (direction == read_HTTP || direction == read_both)
            tcp_socket.async_read_some(
                    buffer(out_buf_),
                    [this, self](boost::system::error_code ec, size_t length) {
                        if (!ec) {
                            string reply;
                            reply.append(out_buf_.data(), out_buf_.data()+length);
                            //cout << "reply:" << reply << endl;
                            do_write(2, length);
                        } else //if (ec != boost::asio::error::eof)
                        {
                            // Most probably remote server closed socket. Let's close both sockets and exit session.
                            _socket.close(); tcp_socket.close();
                        }
                    });
    }

    //void do_write(int direction, size_t Length)
    void do_write(int direction, size_t Length)
    {
        auto self(shared_from_this());

        switch (direction)
        {
            case read_client:     // write to HTTP server, and keep listening from Browser
                async_write(tcp_socket, buffer(in_buf_, Length),
                        [this, self, direction](boost::system::error_code ec, std::size_t length)
                        {
                            if (!ec)
                                do_read(direction);
                            else {
                                // Most probably client closed socket. Let's close both sockets and exit session.
                                _socket.close(); tcp_socket.close();
                            }
                        });
                break;
            case read_HTTP:     // write to Browser, and keep listening from HTTP server
                async_write(_socket, buffer(out_buf_, Length),
                        [this, self, direction](boost::system::error_code ec, std::size_t length)
                        {
                            if (!ec)
                                do_read(direction);
                            else {
                                // Most probably remote server closed socket. Let's close both sockets and exit session.
                                _socket.close(); tcp_socket.close();
                            }
                        });
                break;
        }
    }



};

class SOCKSServer {
private:
    ip::tcp::acceptor _acceptor;
    ip::tcp::socket _socket;

public:
    SOCKSServer(short port)
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

                    //dup2(_socket.native_handle(), STDIN_FILENO);
                    //dup2(_socket.native_handle(), STDOUT_FILENO);
                    //dup2(_socket.native_handle(), STDERR_FILENO);

                    // parse the request
                    make_shared<SOCKSSession>(move(_socket))->start();

                } else {
                    // parent process
                    // inform io_service fork finished (parent)
                    global_io_service.notify_fork(io_service::fork_parent);

                    //cout << "----- " + _socket.remote_endpoint().address().to_string() +
                    //        ":"+ to_string(_socket.remote_endpoint().port()) + " pid:" + to_string(pid) + " -----\n" ;

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

int main(int argc, char* const argv[]) {
    signal(SIGCHLD, SIG_IGN);
    // set port
    short port = PORT;      // default PORT = 7001
    if(argc == 2) {
        port = static_cast<short>(atoi(argv[1]));
    } else if (argc > 2) {
        cerr << "Usage:" << argv[0] << " [port]" << endl;
        return -1;
    }

    try {
        cout << "waiting for connect......" << endl;
        // Socks4Server server(port);
        SOCKSServer server(port);
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
vector<string> cml_split_line(const string& s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}
int Firewall (string ip, string mode)
{
    string line;
    ifstream fconf(config_file);
    // check if the config_file exists
    if(fconf.fail()) {
        cerr << "error: "<< config_file <<" not found\n" ;
        return -1;
    }

    if (fconf)
    {
        while (getline(fconf, line))
        {
            string _ip;
            string _mode;
            vector<string> clear = cml_split_line(line, '#');
            if (clear.empty())
                continue;       // .conf syntax error
            line = clear[0];
            vector<string> parse = cml_split_line(line, ' ');
            if (parse.size() != 3)
                continue;       // .conf syntax error
            _mode = parse[1];
            _ip = parse[2];
            if (_mode == mode) {
                // 完全一樣的ip
                if (_ip == ip) {
                    return 1;
                }
                // 處理mask
                vector<string> _token = cml_split_line(_ip, '.');
                vector<string> token = cml_split_line(ip, '.');
                for (int i = 0; i < 4; i++) {
                    if (_token[i] == "*") {
                        return 1;
                    }
                    if (_token[i] != token[i])
                        break;
                }
            }
        }
        fconf.close();
    }
    return -1;
}