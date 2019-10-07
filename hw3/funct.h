//
// Created by ray on 18年12月4日.
//

#ifndef INC_0756536_NP_PROJECT3_FUNCT_H
#define INC_0756536_NP_PROJECT3_FUNCT_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <iterator>
#include <iostream>
#include <cstdlib>
#include <vector>


using namespace std;

//#define PATH "/home/ray/Ray/NP/Proj3/0756536_np_project3/cmake-build-debug/"
//#define PATH "/net/gcs/107/0756536/ray4452/0756536_np_project3/"
#define PATH ""

struct Header {
    string method;      // GET
    string uri;         // /printenv.cgi
    string path;        // /public_html/***.cgi
    string qstring;     // a=b&c=d
    string protocol;    // HTTP/1.1
    string host;        // nplinux2.cs.nctu.edu.tw
    string connect;     // keep-alive
    string s_addr;      // server_addr
    string s_port;      // server_port
    string r_addr;      // remote_addr
    string r_port;      // remote_port
};

#endif //INC_0756536_NP_PROJECT3_FUNCT_H

void header_print   (Header header);
void parse_req      (const string request, Header *header);
void set_envs       (Header *header);

// HTTP reply
void OK         (string protocol);
void forbidden  (string protocol);
void not_found  (string protocol);
