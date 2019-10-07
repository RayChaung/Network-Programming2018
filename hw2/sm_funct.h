//
// Created by ray on 18年11月13日.
//

#ifndef INC_0756536_NP_PROJECT2_SM_FUNCT_H
#define INC_0756536_NP_PROJECT2_SM_FUNCT_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<iostream>
#include<errno.h>
#include<unistd.h>      //unix std包含UNIX系统服務的函數， ex:read、write、getpid
#include<sys/types.h>   //是Unix/Linux系统的基本系统數據類型的header文件， ex:size_t、time_t、pid_t
#include<sys/wait.h>    //wait、waitid、waitpid...
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<vector>
#include<string>
#include<sys/shm.h>         //Used for shared memory
#include <sys/sem.h>		//Used for semaphores

using namespace std;

#define SHMKEY          ((key_t) 7980)
#define	SEMKEY			((key_t) 2333)  			//Semaphore unique key
#define SHMFLAG         0666    //作為校驗 ,  ubuntu要加

#define MAXCOM          15000   // max length of a single-line input
#define MAX_CMD_COUNT   15000
#define MAX_MSG_SIZE    1024	// one message has at most 1024 bytes
#define MAX_CLIENT	    30
#define MAX_NAME	    20
#define MAX_MSG_TMP	    10	    // one chat buffer has at most 10 unread messages
//#define MAXCMLIST



class Num_pipe{
public:
    Num_pipe(const int num = 0, const int fd_in = STDIN_FILENO, const int fd_out = STDOUT_FILENO){
        this->counter = num;
        this->fd_in = fd_in;
        this->fd_out = fd_out;
    }
    bool sub_counter(){
        if(counter > 0){
            this->counter -= 1;
            return true;
        } else
            return false;
    }
    bool is_counter_zero(){
        return this->counter == 0;
    }
    int get_fd_in(){return fd_in;}
    int get_fd_out(){return fd_out;}
    int get_fd_counter(){return counter;}
    void debugger(){
        cout << "counter:"<< counter << " fd_in:" << fd_in << endl;
    }
private:
    int     counter = 0;
    int     fd_in = STDIN_FILENO;
    int     fd_out = STDOUT_FILENO;
};

typedef struct client {
    int	    id;
    int	    pid;
    int	    port;
    char	addr[INET6_ADDRSTRLEN];
    char	name[MAX_NAME + 1];
    char	msg[MAX_CLIENT][MAX_MSG_TMP][MAX_MSG_SIZE + 1];
} Client;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

#endif //INC_0756536_NP_PROJECT2_SM_FUNCT_H




typedef vector<string> Command;
void cml_loop(struct sockaddr_storage remoteaddr);
int is_num_pipe(const string line, bool *e_flag);
string dele_num_pipe(const string line);
string cml_read_line();
vector<string> cml_split_line(const string& s, char delimiter);
vector<string> cml_split_space(string line);
vector<int> cml_execute(vector<Command> command, bool is_num_pipe,
                        bool is_errpr_pipe, vector<int> real_in, int last_out);
vector<int> execute_cmd_seq(vector<Command> command, bool is_num_pipe, bool is_error_pipe,
                            vector<int> real_in, int last_out);
void args_execute(Command tokens, int fd_in, int fd_out,
                  int pipeline_count, int pipes_fd[][2], bool is_error_pipe);
void sigpipe_handler(int dummy);
void childHandler(int signo);
void broadcast (char *msg);
void initialize();
void welcome();

int to_user_pipe    (string &line);
int from_user_pipe  (string &line, int *fup_id);
int login   (struct sockaddr_storage remoteaddr);
int logout  ();


// build_in
int printenv (int argc, vector<string> argv);
int setupenv (int argc, vector<string> argv);
int name     (int argc, vector<string> argv);
int yell     (int argc, vector<string> argv);
int tell     (int argc, vector<string> argv);
int who      (int argc);

int readline(int fd, char *ptr, int maxlen);
void *get_in_addr(struct sockaddr *sa);
in_port_t get_in_port(struct sockaddr *sa);
void my_bind(int *sockfd, addrinfo **servinfo);
