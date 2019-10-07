//
// Created by ray on 18年11月13日.
//
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>      //unix std包含UNIX系统服務的函數， ex:read、write、getpid
#include<sys/types.h>   //是Unix/Linux系统的基本系统數據類型的header文件， ex:size_t、time_t、pid_t
#include<sys/wait.h>    //wait、waitid、waitpid...
#include<signal.h>
#include<sys/shm.h>         //Used for shared memory
#include <sys/sem.h>		//Used for semaphores
#include<fcntl.h>
#include "sm_funct.h"
#define BACKLOG 30 // 在佇列中可以有多少個連線在等待
#define PORT "7001" // 使用者將連線的 port

using namespace std;

static void sig_handler (int sig);
static void sem_create();
static void shm_create();

int main(int argc, char *argv[]) {

    signal(SIGCHLD, sig_handler);
    signal(SIGINT , sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);

    int sockfd, new_fd;
    int oldstdout, oldstderr;
    char* port = const_cast<char *>("7001");
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage remoteaddr; // 連線者的位址資訊
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

    // create semaphore
    sem_create();

    // create shared memory address
    shm_create();

    while(1) { // 主要的 accept() 迴圈
        addr_size = sizeof remoteaddr;
        new_fd = accept(sockfd, (struct sockaddr *)&remoteaddr, &addr_size);
        if (new_fd == -1) {
            perror("accept");
            //continue;
        }

        inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr *)&remoteaddr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // 這個是 child process
            close(sockfd); // child 不需要 listener
            clearenv ();
            putenv(const_cast<char *>("PATH=bin:."));

            dup2(new_fd,STDIN_FILENO);
            dup2(new_fd,STDOUT_FILENO);
            dup2(new_fd,STDERR_FILENO);

            cml_loop(remoteaddr);
            /*
            if (send(new_fd, "disconnect\n", 12, 0) == -1)
                 perror("send");
            */
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            close(new_fd);
            exit(0);
        }

        close(new_fd); // parent 不需要這個
    }

    exit(EXIT_SUCCESS);
}
void sig_handler (int sig)
{
    if (sig == SIGCHLD) {
        while (waitpid (-1, NULL, WNOHANG) > 0);
    } else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
        int	shmid;
        if ((shmid = shmget (SHMKEY, MAX_CLIENT * sizeof (Client), SHMFLAG)) < 0) {
            perror("shmget");
            exit(1);
        }
        // remove a shared memory (IPC_RMID)
        if (shmctl (shmid, IPC_RMID, nullptr) < 0) {
            perror("shmctl");
            exit(1);
        }
        int semid;
        if ((semid =  semget((key_t)SEMKEY, 1, 0666)) < 0){
            perror("semget");
            exit(1);
        }
        union semun sem_union_delete;
        if (semctl(semid, 0, IPC_RMID, sem_union_delete) == -1){
            perror("semctl");
            exit(1);
        }

        // erase user pipe file
        for (int i = 0; i < MAX_CLIENT; i++) {
            for (int j = 0; j < MAX_CLIENT; j++) {
                char sfile[20] = "", rfile[20] = "";
                sprintf(sfile, "user_pipe/%d->%d", i + 1, j + 1);
                sprintf(rfile, "user_pipe/%d->%d", j + 1, i + 1);
                remove(sfile);
                remove(rfile);
            }
        }
        exit(0);
    }
    signal(sig, sig_handler);
}
void shm_create()
{
    int	shmid = 0;
    Client	*client_set;
    client_set = nullptr;
    // allocate the shared memory , IPC_CREAT :確保開啟的記憶體是新的,而不是現存的記憶體
    if ((shmid = shmget (SHMKEY, MAX_CLIENT * sizeof (Client), SHMFLAG | IPC_CREAT)) < 0) {
        perror("shmget");
        exit(1);
    }
    printf("shmid:%d\n", shmid);
    // attach the allocated shared memory
    if ((client_set = (Client *) shmat (shmid, nullptr, 0)) == (Client *) -1) {
        perror("shmat");
        exit(1);
    }
    // set the allocated shared memory segment to zero
    memset(client_set, 0, MAX_CLIENT * sizeof (Client));
    // detach the shared memory segment
    shmdt(client_set);
}
void sem_create()
{
    int	semid = 0;

    if ((semid = semget((key_t)SEMKEY, 1, 0666 | IPC_CREAT)) < 0){
        perror("semget");
        exit(1);
    }
    printf("semid:%d\n", semid);

    union semun sem_union_init;
    sem_union_init.val = 1;
    if (semctl(semid, 0, SETVAL, sem_union_init) == -1)
    {
        fprintf(stderr, "Creating semaphore failed to initialize\n");
        exit(1);
    }

}