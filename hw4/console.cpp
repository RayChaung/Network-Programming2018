#include <utility>

//
// Created by ray on 18年12月21日.
//

#include <array>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <utility>
#include <unistd.h>

#include "socks4_client.h"

#define MAX_HOST 5
#define HOST_INFORM 3
#define PATH "test_case/"

using namespace std;
using namespace boost::asio;

struct HOST {
    int         id;
    string      hname;      // the hostname or IP of the first server.
    int         port = 0;   // the port of the first server.
    string      fname;      // the file that sent to the first server.
    //ifstream    fin;	    // the batch file for host server input
};
struct SOCKS4 {
    string  address;
    int     port = 0;
    // init with no socks server
    bool    islegal = false;
};

io_service global_io_service;


void set_host       (HOST *hosts);
void set_socks4     (SOCKS4 *socks_server);
void preprint       (HOST hosts[]);
bool is_legel_host  (HOST host);
void socks_not_found(int id);
void shell_not_found(int id);
void output_shell   (int id, string content);
void output_command (int id, string content);

class ShellSession : public enable_shared_from_this<ShellSession> {
private:
    /* ... some data members */
    enum { max_length = 1024};
    HOST _host;
    SOCKS4 _socks_server;
    socks4c::reply socks_reply;
    ifstream fin;
    ip::tcp::resolver::query q;
    ip::tcp::resolver::query socks_q;
    ip::tcp::resolver resolv;
    ip::tcp::socket tcp_socket;
    array<char, max_length> _data;
public:
    ShellSession(HOST host, SOCKS4 socks_server)
            : q(host.hname, to_string(host.port)),
              socks_q(socks_server.address, to_string(socks_server.port)),
              resolv(global_io_service),
              tcp_socket(global_io_service)
              {
                _host = host;
                _socks_server = socks_server;
                fin.open(_host.fname);
              }
    void start() {
        if(_socks_server.islegal)
            do_resolve(socks_q);
        else
            do_resolve(q);
    }
private:
    void do_resolve(ip::tcp::resolver::query real_q) {
        auto self(shared_from_this());      // make sure that ShellSession object outlives the asynchronous operation
        resolv.async_resolve(real_q,
                             [this, self](boost::system::error_code ec, ip::tcp::resolver::iterator it){
                                 if (!ec)
                                     do_connect(it);
                                 else
                                     cerr << "resolve error.\n";
                             });
        //async_resolve(..., do_connect);
    }
    void do_connect(ip::tcp::resolver::iterator it) {
        auto self(shared_from_this());
        tcp_socket.async_connect(*it,
                                 [this, self](boost::system::error_code ec) {
                                     if (!ec) {
                                         // cout << _host.hname << " connect ok" <<endl;
                                         if(_socks_server.islegal) {
                                             // have socks server
                                             // send request
                                             ip::tcp::resolver resolver(global_io_service);
                                             ip::tcp::resolver::query http_query(ip::tcp::v4(), _host.hname, to_string(_host.port));
                                             ip::tcp::endpoint http_endpoint = *resolver.resolve(http_query);
                                             socks4c::request socks_request(socks4c::request::connect, http_endpoint);
                                             write(tcp_socket, socks_request.buffers());
                                             wait_for_reply();
                                         } else
                                            do_read();
                                     } else {
                                         if(_socks_server.islegal)
                                             socks_not_found(_host.id);
                                         else
                                             shell_not_found(_host.id);
                                         cerr << "connect error.\n";
                                         tcp_socket.close();
                                     }

                                 });
        //async_connect(..., do_read);
    }
    void wait_for_reply() {
        auto self(shared_from_this());
        //socks4::reply socks_reply(status, socks_request.endpoint());
        tcp_socket.async_read_some(
                socks_reply.buffers(),
                [this, self](boost::system::error_code ec, size_t /* length */) {
                    if (!ec) {
                        if (socks_reply.status() == socks4c::reply::request_granted) {
                            do_read();
                        } else { /* request_fail */
                            shell_not_found(_host.id);
                            tcp_socket.close();
                        }

                    }
                    else {
                        cerr << "receiv_reply error" << endl;
                        tcp_socket.close();
                    }
                });
    }
    void do_read() {
        auto self(shared_from_this());
        tcp_socket.async_read_some(
                buffer(_data, max_length),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        string command;
                        command.append(_data.data(), _data.data()+length);
                        //cout << "command:" << command << endl;
                        output_shell(_host.id, command);
                        usleep(10000);
                        if (command.find('%') != string::npos) {
                            string inputCmd;
                            getline(fin, inputCmd);
                            inputCmd += "\n";
                            //cout << "inputCmd:" << inputCmd << endl;
                            output_command(_host.id, inputCmd);
                            usleep(10000);
                            do_send_cmd(inputCmd);
                        } else
                            do_read();
                    }
                    else {
                        fin.close();
                    }
                });
    }
    void do_send_cmd(string const& inputCmd) {
        auto self(shared_from_this());

        tcp_socket.async_send(
                buffer(inputCmd),
                [this, self](boost::system::error_code ec, std::size_t /* length */) {
                    if (!ec) {
                        do_read();
                    }
                });
    }

};

int main() {
    /*ip::tcp::resolver::query q{"theboostcpplibraries.com", "80"};
    resolv.async_resolve(q, resolve_handler);
    global_io_service.run();*/
    HOST hosts[MAX_HOST];
    SOCKS4 socks_server;



    set_host(hosts);
    set_socks4(&socks_server);
    preprint(hosts);

    for(int i = 0; i < MAX_HOST; i++)
    {
        if(is_legel_host(hosts[i])) {
            // cout << "host:" << i << " \n";
            make_shared<ShellSession>(hosts[i], socks_server)->start();
        }
    }


    //ip::tcp::resolver::query q{hosts[0].hname, to_string(hosts[0].port)};
    //resolv.async_resolve(q, resolve_handler);
    global_io_service.run();

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

bool is_legel_host(HOST host)
{
    return host.hname.c_str() && host.port && host.fname.c_str();
}

void host_print(HOST host[])
{
    int count = 0;
    for(int i = 0; i< MAX_HOST; i++) {
        if (host[i].hname.size())
            count ++;
        cout << "h_name:\t\t" << host[i].hname << endl;
        cout << "port:\t\t" << host[i].port << endl;
        cout << "fname:\t\t" << host[i].fname << endl;
    }
    cout << "host_number: " << count << endl;
}

// parse socks4 server from query_string
void set_socks4(SOCKS4 *socks_server)
{
    string qstring;
    qstring = getenv("QUERY_STRING");
    vector<string> spl_qstring = cml_split_line(qstring, '&');
    // &sh=&sp=
    vector<string> split = cml_split_line(spl_qstring[MAX_HOST * HOST_INFORM], '=');
    if (split.size() == 2) {
        socks_server->address = split[1];
        vector<string> p_split = cml_split_line(spl_qstring[MAX_HOST * HOST_INFORM + 1], '=');
        if (p_split.size() == 2) {
            socks_server->address = split[1];
            socks_server->port = stoi(p_split[1]);
            socks_server->islegal = true;
        }
    }
}
void set_host(HOST *hosts)
{
    string qstring;
    qstring = getenv("QUERY_STRING");
    vector<string> spl_qstring = cml_split_line(qstring, '&');
    for (int i = 0; i< MAX_HOST; i++)
    {
        hosts[i].id = i;
        // h0=npbsd1.cs.nctu.edu.tw&
        vector<string> split = cml_split_line(spl_qstring[i*HOST_INFORM], '=');
        if (split.size() == 2)
            hosts[i].hname = split[1];
        // p0=8787&
        split = cml_split_line(spl_qstring[i*HOST_INFORM + 1], '=');
        if (split.size() == 2)
            hosts[i].port   = stoi (split[1]);
        // f0=t1.txt
        split = cml_split_line(spl_qstring[i*HOST_INFORM + 2], '=');
        if (split.size() == 2) {
            hosts[i].fname = PATH + split[1];
            /*if ((fopen (hosts[i].fname.c_str(), "r")) == NULL) {
                cout << "error: failed to open file " << hosts[i].fname << endl;
            }*/
        }
    }

}

void preprint(HOST hosts[])
{
    string reply = "Content-Type: text/html\n\n"
                   "<!DOCTYPE html>\n"
                   "<html lang=\"en\">\n"
                   "  <head>\n"
                   "    <meta charset=\"UTF-8\" />\n"
                   "    <title>NP Project 3 Console</title>\n"
                   "    <link\n"
                   "      rel=\"stylesheet\"\n"
                   "      href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n"
                   "      integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n"
                   "      crossorigin=\"anonymous\"\n"
                   "    />\n"
                   "    <link\n"
                   "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
                   "      rel=\"stylesheet\"\n"
                   "    />\n"
                   "    <link\n"
                   "      rel=\"icon\"\n"
                   "      type=\"image/png\"\n"
                   "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
                   "    />\n"
                   "    <style>\n"
                   "      * {\n"
                   "        font-family: 'Source Code Pro', monospace;\n"
                   "        font-size: 1rem !important;\n"
                   "      }\n"
                   "      body {\n"
                   "        background-color: #212529;\n"
                   "      }\n"
                   "      pre {\n"
                   "        color: #cccccc;\n"
                   "      }\n"
                   "      b {\n"
                   "        color: #ffffff;\n"
                   "      }\n"
                   "    </style>\n"
                   "  </head>\n"
                   "  <body>\n"
                   "    <table class=\"table table-dark table-bordered\">\n"
                   "      <thead>\n"
                   "        <tr>\n";
    for(int i = 0; i < MAX_HOST; i++) {
        if (is_legel_host(hosts[i]))
            reply += "          <th scope=\"col\">" + hosts[i].hname + ":" + to_string(hosts[i].port) + "</th>\n";
    }
    reply +=       "        </tr>\n"
                   "      </thead>\n"
                   "      <tbody>\n"
                   "        <tr>\n";
    for(int i = 0; i < MAX_HOST; i++) {
        if (is_legel_host(hosts[i]))
            reply += "          <td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>\n";
    }
    reply +=       "        </tr>\n"
                   "      </tbody>\n"
                   "    </table>\n"
                   "  </body>\n"
                   "</html>";
    cout << reply << endl << flush;
}

string html_escape(const string& str) {

    string escaped;

    for (auto&& ch : str) escaped += ("&#" + to_string(int(ch)) + ";");

    return escaped;

}
// 999 SOCKS4 server Not Found
void socks_not_found (int id)
{
    string html = html_escape("999 SOCKS4 server Not Found");
    string reply = "<script>document.getElementById('s" + to_string(id) + "').innerHTML += \"" + html + "\";</script>\")";
    cout << reply << flush;
}
// 998 Shell server Not Found
void shell_not_found (int id)
{
    string html = html_escape("998 Shell server Not Found");
    string reply = "<script>document.getElementById('s" + to_string(id) + "').innerHTML += \"" + html + "\";</script>\")";
    cout << reply << flush;
}

void output_shell(int id, string content) {
    string html = html_escape(content);
    // <script>document.getElementById('s0').innerHTML += '% ';</script>
    string reply = "<script>document.getElementById('s" + to_string(id) + "').innerHTML += \"" + html + "\";</script>\")";
    cout << reply << flush;
}

void output_command(int id, string content) {
    string html = html_escape(content);
    // <script>document.getElementById('s0').innerHTML += '<b>ls&NewLine;</b>';</script>
    string reply = "<script>document.getElementById('s" + to_string(id) + "').innerHTML += \"<b>" + html + "</b>\";</script>\")";
    cout << reply << flush;
}