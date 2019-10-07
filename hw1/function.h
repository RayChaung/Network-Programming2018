//
// Created by ray on 18年10月6日.
//

#ifndef INC_0756536_NP_PROJECT1_FUNCTION_H
#define INC_0756536_NP_PROJECT1_FUNCTION_H

#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<vector>
#include<string>

using namespace std;

#define MAXCOM 15000    // max length of a single-line input
#define MAXBUILTIN 3
#define MAX_CMD_COUNT 15000
//#define MAXCMLIST

#endif //INC_0756536_NP_PROJECT1_FUNCTION_H

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
    int counter = 0;
    int fd_in = 0;
    int fd_out = 1;
};

class Built_in{
public:
    virtual int operation(vector<string> tokens){}
    string name;
};

class Cml_setenv :public Built_in{
public:
    Cml_setenv(string s){name = s;}
    virtual int operation(vector<string> tokens){
        //假設會執行 overwrite
        setenv(tokens[0].c_str(), tokens[1].c_str(), 1);
    }
};

class Cml_printenv :public Built_in{
public:
    Cml_printenv(string s){name = s;}
    virtual int operation(vector<string> tokens){
        //check if have such env variable
        if(getenv(tokens[0].c_str())){
            printf ("%s\n",getenv(tokens[0].c_str()));
        }
    }
};

class Cml_exit :public Built_in{
public:
    Cml_exit(string s){name = s;}
    virtual int operation(vector<string> tokens){
        exit(EXIT_SUCCESS);
    }
};

typedef vector<string> Command;
void cml_loop();
int is_num_pipe(const string line, bool *e_flag);
string cml_read_line();
string dele_num_pipe(const string line);
vector<string> cml_split_line(const string& s, char delimiter);
vector<string> cml_split_space(string line);
vector<int> cml_execute(vector<Command> command, Built_in** built_in
        , bool is_num_pipe, bool is_errpr_pipe, vector<int> real_in, int last_out);
vector<int> execute_cmd_seq(vector<Command> command, bool is_num_pipe, bool is_error_pipe,
                            vector<int> real_in, int last_out);
void args_execute(Command tokens, int fd_in, int fd_out,
                  int pipeline_count, int pipes_fd[][2], bool is_error_pipe);
void sigpipe_handler(int dummy);
void childHandler(int signo);