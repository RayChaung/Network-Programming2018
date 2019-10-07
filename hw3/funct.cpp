//
// Created by ray on 18年12月4日.
//

#include "funct.h"

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

void header_print(Header header)
{
    cout << "method:\t\t" << header.method << endl;
    cout << "uri:\t\t" << header.uri << endl;
    cout << "path:\t\t" << header.path << endl;
    cout << "q_string:\t" << header.qstring << endl;
    cout << "protocol:\t" << header.protocol << endl;
    cout << "host:\t\t" << header.host << endl;
    cout << "connect:\t" << header.connect << endl;
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
    vector<string> path = cml_split_line(uri_qstring[0], '/');
    header->uri = path[1];           // URI
    header->path = PATH + header->uri;      // PATH
    if(uri_qstring.size() == 2) {
        // 若有qstring
        header->qstring = uri_qstring[1];   // qstring
    }

    header->protocol = first_line[2];       // HTTP/1.1

    // second line HOST
    vector<string> second_line;
    second_line = cml_split_line(target[1], ' ');

    if(second_line.size() == 2) {
        vector<string> host_port;
        host_port = cml_split_line(second_line[1], ':');
        header->host = host_port[0];
    }

    // third line Connect
    vector<string> third_line;
    third_line = cml_split_line(target[2], ' ');
    header->connect = third_line[1];
}

void set_envs (Header *header)
{
    clearenv ();
    setenv ("SERVER_SOFTWARE", "lighttpd/1.4.50", 1);
    setenv ("REDIRECT_STATUS", "200", 1);
    //setenv ("DOCUMENT_ROOT", PATH, 1);
    setenv ("REQUEST_METHOD", header->method.c_str(), 1);
    setenv ("REQUEST_URI", header->uri.c_str(), 1);
    setenv ("QUERY_STRING", header->qstring.c_str(), 1);
    setenv ("SERVER_PROTOCOL", header->protocol.c_str(), 1);
    if (header->host.c_str()) {
        setenv ("HTTP_HOST", header->host.c_str(), 1);
        setenv ("SERVER_NAME", header->host.c_str(), 1);
    }
    //if (header->port.c_str())
    //    setenv ("SERVER_PORT", header->port.c_str(), 1);
    if (header->connect.c_str())
        setenv ("HTTP_CONNECTION", header->connect.c_str(), 1);
    if (header->s_port.c_str())
        setenv ("SERVER_PORT", header->s_port.c_str(), 1);
    if (header->r_addr.c_str())
        setenv ("SERVER_ADDR", header->s_addr.c_str(), 1);
    if (header->r_port.c_str())
        setenv ("REMOTE_PORT", header->r_port.c_str(), 1);
    if (header->r_addr.c_str())
        setenv ("REMOTE_ADDR", header->r_addr.c_str(), 1);
}

// 200 OK
void OK (string protocol)
{
    // HTTP/1.1 200 OK \n
    string reply = protocol +
                   " 200 OK\n";
    cout << reply << flush;
}

// 403 Forbidden
void forbidden (string protocol)
{

    string reply = protocol +
                   " 403 Forbidden\n"
                   "Content-Type: text/html\n\n"
                   "<html>\n"
                   "<head><title>403 Forbidden</title></head>\n"
                   "<body><h1>403 Forbidden</h1></body>\n"
                   "</html>\n";
    cout << reply << flush;
}

// 404 Not Found
void not_found (string protocol)
{
    string reply = protocol +
                   " 404 Not Found\n"
                   "Content-Type: text/html\n\n"
                   "<html>\n"
                   "<head><title>404 Not Found</title></head>\n"
                   "<body><h1>404 Not Found</h1></body>\n"
                   "</html>\n";
    cout << reply << flush;
}