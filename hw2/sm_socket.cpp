//
// Created by ray on 18年11月13日.
//

#include <vector>
#include "sm_funct.h"

//send Welcome message
void welcome()
{
    char *msg = "****************************************\n"
                "** Welcome to the information server. **\n"
                "****************************************\n";
    size_t len;
    len = strlen(msg);
    send(STDOUT_FILENO, msg, len, 0);
}

// get port, IPv4 or IPv6:
in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

// 取得sockaddr，IPv4或IPv6：
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void my_bind(int *sockfd, addrinfo **servinfo)
{
    struct addrinfo *p;
    int yes=1;
    // 以迴圈找出全部的結果，並 bind 到第一個能用的結果
    for(p = *servinfo; p != NULL; p = p->ai_next) {
        if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: bind");
            close(*sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(*servinfo); // 全部都用這個 structure

}

//built_in
int printenv (int argc, vector<string> argv)
{
    if(getenv(argv[1].c_str())){
        printf ("%s\n",getenv(argv[1].c_str()));
        fflush(stdout);
    }
    return 1;
}
int setupenv (int argc, vector<string> argv)
{
    if (argc == 3) {
        //假設會執行 overwrite
        setenv(argv[1].c_str(), argv[2].c_str(), 1);
        return 1;
    } else
        fputs ("Usage: setenv <name> <value>\n", stderr);
    return 1;
}