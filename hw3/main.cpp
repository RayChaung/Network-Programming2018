#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <string>
#include <sstream>
#include <iterator>
#include <cstdlib>
#include <vector>

#define PORT		7001	// default port
#define MAXLENGTH	3000
#define MAXTXT		10
#define MAXHOST		5
#define HOST_INFORM 3
#define PATH		"test_case\\"
//#define PATH		"C:\\Users\\Ray\\Documents\\Ray\\V1\\NP\\Proj3\\0756536_np_project3\\cgi_server\\Debug\\test_case\\"

using namespace std;
using namespace boost::asio;

struct Header {
	string method;      // GET
	string uri;         // /printenv.cgi
	//string path;        // /public_html/***.cgi
	string qstring;     // a=b&c=d
	string protocol;    // HTTP/1.1
	string host;        // nplinux2.cs.nctu.edu.tw
	string connect;     // keep-alive
	string sv_addr;      // server_addr
	string sv_port;      // server_port
	string rm_addr;      // remote_addr
	string rm_port;      // remote_port
};
struct Host {
	int         id;
	string      hname;      // the hostname or IP of the first server.
	int         port = 0;   // the port of the first server.
	string      fname;      // the file that sent to the first server.
};


io_service global_io_service;

void parse_req		(const string request, Header *header);
void header_print	(Header header)
{
	cout << "method:\t\t" << header.method << endl;
	cout << "uri:\t\t" << header.uri << endl;
	cout << "q_string:\t" << header.qstring << endl;
	cout << "protocol:\t" << header.protocol << endl;
	cout << "host:\t\t" << header.host << endl;
	cout << "connect:\t" << header.connect << endl;
	cout << "server:\t\t" << header.sv_addr << ":" << header.sv_port << endl;
	cout << "client:\t\t" << header.rm_addr << ":" << header.rm_port << endl;
}
string OK			(string protocol);
string not_found	(string protocol);
string forbidden	(string protocol);
string panel_cgi	(Header header);
// for console.cgi
string preprint		(Host hosts[]);
void set_host		(Host *hosts, Header header);
void host_print		(Host host[]);
bool is_legel_host	(Host host);
string output_shell (int id, string content);
string output_command(int id, string content);



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
				string request;
				request.append(_data.data(), _data.data() + length);

				header.sv_port = to_string(_socket.local_endpoint().port());
				header.sv_addr = _socket.local_endpoint().address().to_string();
				header.rm_port = to_string(_socket.remote_endpoint().port());
				header.rm_addr = _socket.remote_endpoint().address().to_string();

				parse_req(request, &header);
				//header_print(header);
				// matching with ***.cgi 
				if (header.uri == "/panel.cgi") {
					string reply = OK(header.protocol);
					do_write(reply);
					reply = panel_cgi(header);
					do_write(reply);
				}
				else if (header.uri == "/console.cgi") {
					string reply = OK(header.protocol);
					do_write(reply);
					Host hosts[MAXHOST];
					set_host(hosts, header);
					reply = preprint(hosts);
					do_write(reply);
					host_print(hosts);
					for (int i = 0; i < MAXHOST; i++)
					{
						if (is_legel_host(hosts[i])) {
							// cout << "host:" << i << " \n";
							make_shared<ShellSession>(hosts[i], shared_from_this())->start();
						}
					}
				}
				else {
					string reply = not_found(header.protocol);
					do_write(reply);
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
// Nested Class of ShellSession
private:
	class ShellSession : public enable_shared_from_this<ShellSession> {
	private:
		/* ... some data members */
		enum { max_length = 1024 };
		Host _host;
		shared_ptr<HTTPSession> http_ptr;
		ifstream fin;
		ip::tcp::resolver::query q;
		ip::tcp::resolver resolv;
		ip::tcp::socket tcp_socket;
		array<char, max_length> _data;
	public:
		ShellSession(Host host, const shared_ptr<HTTPSession>& _ptr)
			: q(host.hname, to_string(host.port)),
			resolv(global_io_service),
			tcp_socket(global_io_service),
			http_ptr(_ptr) {
			_host = host;
			fin.open(_host.fname);
		}
		void start() {
			do_resolve();
		}
	private:
		void do_resolve() {
			auto self(shared_from_this());      // make sure that ShellSession object outlives the asynchronous operation
			resolv.async_resolve(q,
				[this, self](boost::system::error_code ec, ip::tcp::resolver::iterator it) {
				if (!ec) {
					do_connect(it);
				}
			});
			//async_resolve(..., do_connect);
		}
		void do_connect(ip::tcp::resolver::iterator it) {
			auto self(shared_from_this());
			tcp_socket.async_connect(*it,
				[this, self](boost::system::error_code ec) {
				if (!ec) {
					//cout << _host.hname << ":" << to_string(_host.port) << "connect ok\n";
					do_shell_read();
				}
			});
			//async_connect(..., do_read);
		}
		void do_shell_read() {
			auto self(shared_from_this());
			tcp_socket.async_read_some(
				buffer(_data, max_length),
				[this, self](boost::system::error_code ec, std::size_t length) {
				if (!ec) {
					string command;
					command.append(_data.data(), _data.data() + length);
					//cout << command << endl;
					http_ptr->do_write(output_shell(_host.id, command));
					if (command.find('%') != string::npos) {
						string inputCmd;
						getline(fin, inputCmd);
						inputCmd += "\n";
						//cout << inputCmd << endl;
						http_ptr->do_write(output_command(_host.id, inputCmd));
						do_send_cmd(inputCmd);
					}
					else
						do_shell_read();
				}
				else {
					fin.close();
				}
			});
		}
		void do_send_cmd(string const& inputCmd) {
			auto self(shared_from_this());
			tcp_socket.async_send(
				buffer(inputCmd.c_str(), inputCmd.size()),
				[this, self](boost::system::error_code ec, std::size_t /* length */) {
				if (!ec) {
					do_shell_read();
				}
			});
		}
	};

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
			if (!ec) make_shared<HTTPSession>(move(_socket))->start();
			do_accept();
		});
	}
};

int main(int argc, char *argv[])
{

	// set port
	short port = PORT;      // default PORT = 80
	if (argc == 2) {
		port = static_cast<short>(atoi(argv[1]));
	}
	else if (argc > 2) {
		cerr << "Usage:" << argv[0] << " [port]" << endl;
		return -1;
	}
	
	try {
		//EchoServer server(port);
		HTTPServer server(port);
		global_io_service.run();
	}
	catch (exception& e) {
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
void parse_req(const string request, Header *header)
{
	vector<string> target;
	target = cml_split_line(request, static_cast<char>('\r\n'));
	/*
	int counter = 0;
	for (auto i = target.begin(); i != target.end(); i++)
	{
		cout << "counter = " << counter << " " << *i << endl;
		counter ++;
	}
	*/
	// first line
	vector<string> first_line;
	first_line = cml_split_line(target[0], ' ');
	header->method = first_line[0];         // GET

	vector<string> uri_qstring;
	uri_qstring = cml_split_line(first_line[1], '?');
	header->uri = uri_qstring[0];           // URI
	//header->path = PATH + header->uri;      // PATH
	if (uri_qstring.size() == 2) {
		// ­Y¦³qstring
		header->qstring = uri_qstring[1];   // qstring
	}

	header->protocol = first_line[2];       // HTTP/1.1

	// second line HOST
	vector<string> second_line;
	second_line = cml_split_line(target[1], ' ');

	if (second_line.size() == 2) {
		vector<string> host_port;
		host_port = cml_split_line(second_line[1], ':');
		header->host = host_port[0];
	}

	// third line Connect
	vector<string> third_line;
	third_line = cml_split_line(target[2], ' ');
	header->connect = third_line[1];
}

// 200 OK
string OK(string protocol)
{
	string reply = protocol +
		" 200 OK\n";
	return reply;
}
// 403 Forbidden
string forbidden(string protocol)
{

	string reply = protocol +
		" 403 Forbidden\n"
		"Content-Type: text/html\n\n"
		"<html>\n"
		"<head><title>403 Forbidden</title></head>\n"
		"<body><h1>403 Forbidden</h1></body>\n"
		"</html>\n";
	return reply;
}
// 404 Not Found
string not_found(string protocol)
{
	string reply = protocol +
		" 404 Not Found\n"
		"Content-Type: text/html\n\n"
		"<html>\n"
		"<head><title>404 Not Found</title></head>\n"
		"<body><h1>404 Not Found</h1></body>\n"
		"</html>\n";
	return reply;
}

string panel_cgi(Header header)
{
	string action = "console.cgi";
	string test_case_menu = "";
	for (int i = 0; i < MAXTXT; i++) {
		string txt = "t" + to_string(i+1) + ".txt";
		test_case_menu += "<option value=\"" + txt +"\">" + txt + "</option>";
	}
	string host_menu = "";
	for (int i = 0; i < MAXHOST; i++) 
		host_menu += "<option value=\"nplinux" + to_string(i+1) + ".cs.nctu.edu.tw\">nplinux" + to_string(i+1) + "</option>";
	for (int i = 0; i < MAXHOST; i++) 
		host_menu += "<option value=\"npbsd" + to_string(i+1) + ".cs.nctu.edu.tw\">npbsd" + to_string(i+1) + "</option>";
	string reply = "Content-type: text/html\r\n\r\n"
				   "<!DOCTYPE html>\n"
                   "<html lang=\"en\">\n"
                   "  <head>\n"
                   "    <title>NP Project 3 Panel</title>\n"
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
                   "      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n"
                   "    />\n"
                   "    <style>\n"
                   "      * {\n"
                   "        font-family: 'Source Code Pro', monospace;\n"
                   "      }\n"
                   "    </style>\n"
                   "  </head>\n"
				   "  <body class=\"bg-secondary pt-5\">";
	reply +=	   "<form action=\"" + action + "\" method=\"" + header.method + "\">\n"
                   "      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n"
                   "        <thead class=\"thead-dark\">\n"
                   "          <tr>\n"
                   "            <th scope=\"col\">#</th>\n"
                   "            <th scope=\"col\">Host</th>\n"
                   "            <th scope=\"col\">Port</th>\n"
                   "            <th scope=\"col\">Input File</th>\n"
                   "          </tr>\n"
                   "        </thead>\n"
                   "        <tbody>";
	for (int i = 0; i < MAXHOST; i++) {
		reply +=   "          <tr>\n"
                   "            <th scope=\"row\" class=\"align-middle\">Session " + to_string(i+1) + "</th>\n"
                   "            <td>\n"
                   "              <div class=\"input-group\">\n"
                   "                <select name=\"h" + to_string(i) + "\" class=\"custom-select\">\n"
                   "                  <option></option>" + host_menu + "\n"
                   "                </select>\n"
                   "                <div class=\"input-group-append\">\n"
                   "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
                   "                </div>\n"
                   "              </div>\n"
                   "            </td>\n"
                   "            <td>\n"
                   "              <input name=\"p" + to_string(i) + "\" type=\"text\" class=\"form-control\" size=\"5\" />\n"
                   "            </td>\n"
                   "            <td>\n"
                   "              <select name=\"f" + to_string(i) + "\" class=\"custom-select\">\n"
                   "                <option></option>\n"
                   "                " + test_case_menu + "\n"
                   "              </select>\n"
                   "            </td>\n"
				   "          </tr>";
	}
	reply +=	   "<tr>\n"
                   "            <td colspan=\"3\"></td>\n"
                   "            <td>\n"
                   "              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n"
                   "            </td>\n"
                   "          </tr>\n"
                   "        </tbody>\n"
                   "      </table>\n"
                   "    </form>\n"
                   "  </body>\n"
                   "</html>";
	return reply;
}

void host_print(Host host[])
{
	int count = 0;
	for (int i = 0; i < MAXHOST; i++) {
		if (is_legel_host(host[i]))
			count++;
		cout << "h_name:\t\t" << host[i].hname << endl;
		cout << "port:\t\t" << host[i].port << endl;
		cout << "fname:\t\t" << host[i].fname << endl;
	}
	cout << "host_number: " << count << endl;
}

void set_host(Host *hosts, Header header)
{
	string qstring;
	qstring = header.qstring;
	vector<string> spl_qstring = cml_split_line(qstring, '&');
	for (int i = 0; i < MAXHOST; i++)
	{
		hosts[i].id = i;
		// h0=npbsd1.cs.nctu.edu.tw&
		vector<string> split = cml_split_line(spl_qstring[i*HOST_INFORM], '=');
		if (split.size() == 2)
			hosts[i].hname = split[1];
		// p0=8787&
		split = cml_split_line(spl_qstring[i*HOST_INFORM + 1], '=');
		if (split.size() == 2)
			hosts[i].port = stoi(split[1]);
		// f0=t1.txt
		split = cml_split_line(spl_qstring[i*HOST_INFORM + 2], '=');
		if (split.size() == 2) {
			hosts[i].fname = PATH + split[1];
			/*if ((fopen(hosts[i].fname.c_str(), "rb")) == NULL) {
				cout << "error: failed to open file " << hosts[i].fname << endl;
			}*/
		}
	}
}
bool is_legel_host(Host host)
{
	return host.hname.size() && host.port && host.fname.c_str();
}
string preprint(Host hosts[])
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
	for (int i = 0; i < MAXHOST; i++) {
		if (is_legel_host(hosts[i]))
			reply += "          <th scope=\"col\">" + hosts[i].hname + ":" + to_string(hosts[i].port) + "</th>\n";
	}
	reply += "        </tr>\n"
		"      </thead>\n"
		"      <tbody>\n"
		"        <tr>\n";
	for (int i = 0; i < MAXHOST; i++) {
		if (is_legel_host(hosts[i]))
			reply += "          <td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>\n";
	}
	reply += "        </tr>\n"
		"      </tbody>\n"
		"    </table>\n"
		"  </body>\n"
		"</html>";
	return reply;
}
void escape(string& data) {
	string buffer;
	buffer.reserve(data.size());
	for (size_t pos = 0; pos != data.size(); ++pos) {
		switch (data[pos]) {
		case '&':  buffer.append("&amp;");       break;
		case '\"': buffer.append("&quot;");      break;
		case '\'': buffer.append("&apos;");      break;
		case '<':  buffer.append("&lt;");        break;
		case '>':  buffer.append("&gt;");        break;
		case '\n': buffer.append("&NewLine;");   break;
		case '\r': buffer.append("");            break;
		default:   buffer.append(&data[pos], 1); break;
		}
	}
	data.swap(buffer);
}
string output_shell(int id, string content) {
	escape(content);
	// <script>document.getElementById('s0').innerHTML += '% ';</script>
	string reply = "<script>document.getElementById('s" + to_string(id) + "').innerHTML += \"" + content + "\";</script>\")";
	return reply;
}
string output_command(int id, string content) {
	escape(content);
	// <script>document.getElementById('s0').innerHTML += '<b>ls&NewLine;</b>';</script>
	string reply = "<script>document.getElementById('s" + to_string(id) + "').innerHTML += \"<b>" + content + "</b>\";</script>\")";
	return reply;
}