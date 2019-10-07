#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>      //unix std包含UNIX系统服務的函數， ex:read、write、getpid
#include<sys/types.h>   //是Unix/Linux系统的基本系统數據類型的header文件， ex:size_t、time_t、pid_t
#include<sys/wait.h>    //wait、waitid、waitpid...
#include<signal.h>
#include "function.h"
#define BACKLOG 1 // 在佇列中可以有多少個連線在等待
#define PORT "7001" // 使用者將連線的 port

using namespace std;



int main(int argc, char *argv[]) {

    int sockfd, new_fd;
    int oldstdout, oldstderr;
    char* port = "7001";
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // 連線者的位址資訊
    socklen_t addr_size;

    char s[INET6_ADDRSTRLEN];
    int rv;

    if(argv[1])
    {
        port = argv[1];
    }

    memset(&hints, 0, sizeof hints); // 確保struct為空
    hints.ai_family = AF_UNSPEC; //  use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    // 準備好連線
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    //bind 到第一個能用的結果,且listen
    my_bind(&sockfd, &servinfo);

    //listen
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections...\n");



    while(1) { // 主要的 accept() 迴圈
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {
            perror("accept");
            //continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        //if (!fork()) { // 這個是 child process
        //close(sockfd); // child 不需要 listener
        putenv(const_cast<char *>("PATH=bin:."));

        //創建STDOUT & STDERR 的備份
        oldstdout = dup(STDOUT_FILENO);
        oldstderr = dup(STDERR_FILENO);

        dup2(new_fd,STDOUT_FILENO);
        dup2(new_fd,STDERR_FILENO);


        cml_loop(new_fd);
        /*
        if (send(new_fd, "disconnect\n", 12, 0) == -1)
             perror("send");
        */
        dup2(oldstdout,STDOUT_FILENO);
        dup2(oldstderr,STDERR_FILENO);
        close(oldstdout);
        close(oldstderr);

        close(new_fd);
        //exit(0);
        //}
        //close(new_fd); // parent 不需要這個
    }
    close(sockfd);
    exit(EXIT_SUCCESS);
}