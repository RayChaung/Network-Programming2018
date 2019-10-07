//
// Created by ray on 18年10月6日.
//
#include<sstream>
#include<iterator>
#include<cstring>
#include<unistd.h>
#include<errno.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/wait.h>    //wait、waitid、waitpid...
#include<sys/stat.h>
#include<fcntl.h>
//#include<readline/readline.h>
//#include<boost/algorithm/string.hpp>
//#include<boost/algorithm/string/classification.hpp>

#include "function.h"

using namespace std;

vector<pid_t> pid_table;

int readline(int fd, char *ptr, int maxlen)
{
    int n,rc;
    char c;
    *ptr = 0;
    for(n=1;n<maxlen;n++)
    {
        if((rc=read(fd,&c,1)) == 1)
        {
            if(c=='\n')  break;
            *ptr++ = c;
        }
        else if(rc==0)
        {
            if(n==1)     return(0);
            else         break;
        }
        else
            return(-1);
    }
    *ptr = 0;
    return(n);
}

void cml_loop(int sockfd)
{
    Cml_setenv setenv_("setenv");
    Cml_printenv printenv_("printenv");
    Cml_exit exit_("exit");
    Built_in *built_in[MAXBUILTIN] = {&setenv_, &printenv_, &exit_};
    vector<Num_pipe> num_pipe_list;
    int status = 1;
    while(status)
    {
        int n_flag ;
        bool e_flag = false;
        vector<int> fd_in_out = { STDIN_FILENO, STDOUT_FILENO};
        int last_out = STDOUT_FILENO;
        string line;
        vector<string> tokens;
        vector<Command> command;

        printf("%% ");
        fflush(stdout);


        auto *buffer = (char*)malloc(sizeof(char) * MAXCOM);
        int rc;

        rc = readline(sockfd, buffer, MAXCOM);
        if(rc == 0) {   // connection terminated
            exit(0);
        }
        else if(rc < 0){
            perror("receive");
            exit(1);
        }

        line = string(buffer);

        //size = 1 represent line only has '\0'
        if(line.size()!=1) {
            n_flag = is_num_pipe(line , &e_flag);
            //not null command
            for (auto P = num_pipe_list.begin(); P != num_pipe_list.end();) {
                if (P->sub_counter()) {
                    if(P->is_counter_zero() ){
                        fd_in_out[0] = P->get_fd_in();
                        last_out = P->get_fd_out();
                        num_pipe_list.erase(P);
                    } else
                        P++;
                } else{
                    P++;
                }
            }
            for(auto &p : num_pipe_list){
                //p.debugger();
                if(n_flag && n_flag == p.get_fd_counter()) {
                    //進同一個pipe
                    fd_in_out[1] = p.get_fd_out();
                    //flag清空，此command蛻變為一般command僅output進pipe(非stdout)
                    n_flag = 0;
                    line = dele_num_pipe(line);
                    //cout << line << endl ;
                    break;
                }
            }
            //cout << "flag:" << flag;
            //cout << " should in:" << fd_in_out[0] << " out:" << fd_in_out[1] << endl;
            /*若不是number pipe直接執行*/
            if (!n_flag) {
                tokens = cml_split_line(line, '|');
                //cout << "full cml:" << line << endl;
                for (auto const &a : tokens) {
                    command.push_back(cml_split_space(a));
                }

                status = cml_execute(command, built_in, false, e_flag, fd_in_out, last_out)[2];
            } else {
                /*number pipe先暫存在command_list中*/
                tokens = cml_split_line(dele_num_pipe(line), '|');
                for (auto const &a : tokens) {
                    command.push_back(cml_split_space(a));
                }
                vector<int> debugger = cml_execute(command, built_in, true, e_flag, fd_in_out, last_out);
                status = debugger[2];
                //cout << "here will implement number pipe\n";
                //cout << dele_num_pipe(line) << "num: " << flag << endl;
                //cout << " in_pipe_num: " << debugger[0] << endl;
                //cout << " out_pipe_num: " << debugger[1] << endl;
                Num_pipe num_pipe(n_flag, debugger[1], debugger[0]);
                num_pipe_list.push_back(num_pipe);

            }

        }
    }
}

/*
void cml_loop()
{
    Cml_setenv setenv_("setenv");
    Cml_printenv printenv_("printenv");
    Cml_exit exit_("exit");
    Built_in *built_in[MAXBUILTIN] = {&setenv_, &printenv_, &exit_};
    vector<Num_pipe> num_pipe_list;
    //int status = 1;
    while(true)
    {
        int n_flag ;
        bool e_flag = false;
        vector<int> fd_in_out = { STDIN_FILENO, STDOUT_FILENO};
        int last_out = STDOUT_FILENO;
        string line;
        vector<string> tokens;
        vector<Command> command;
        printf("%% ");
        line = cml_read_line();
        if(!line.empty()) {
            n_flag = is_num_pipe(line , &e_flag);
            //not null command
            for (auto P = num_pipe_list.begin(); P != num_pipe_list.end();) {
                if (P->sub_counter()) {
                    if(P->is_counter_zero() ){
                        fd_in_out[0] = P->get_fd_in();
                        last_out = P->get_fd_out();
                        num_pipe_list.erase(P);
                    } else
                        P++;
                } else{
                    P++;
                }
            }
            for(auto &p : num_pipe_list){
                //p.debugger();
                if(n_flag && n_flag == p.get_fd_counter()) {
                    //進同一個pipe
                    fd_in_out[1] = p.get_fd_out();
                    //flag清空，此command蛻變為一般command僅output進pipe(非stdout)
                    n_flag = 0;
                    line = dele_num_pipe(line);
                    //cout << line << endl ;
                    break;
                }
            }
            //cout << "flag:" << flag;
            //cout << " should in:" << fd_in_out[0] << " out:" << fd_in_out[1] << endl;
            //若不是number pipe直接執行
            if (!n_flag) {
                tokens = cml_split_line(line, '|');
                //cout << "full cml:" << line << endl;
                for (auto const &a : tokens) {
                    command.push_back(cml_split_space(a));
                }

                cml_execute(command, built_in, false, e_flag, fd_in_out, last_out);
            } else {
                //number pipe先暫存在command_list中
                tokens = cml_split_line(dele_num_pipe(line), '|');
                for (auto const &a : tokens) {
                    command.push_back(cml_split_space(a));
                }
                vector<int> debugger = cml_execute(command, built_in, true, e_flag, fd_in_out, last_out);
                //cout << "here will implement number pipe\n";
                //cout << dele_num_pipe(line) << "num: " << flag << endl;
                //cout << " in_pipe_num: " << debugger[0] << endl;
                //cout << " out_pipe_num: " << debugger[1] << endl;
                Num_pipe num_pipe(n_flag, debugger[1], debugger[0]);
                num_pipe_list.push_back(num_pipe);

            }

        }
    }
}*/

string dele_num_pipe(const string line)
{
    vector<string> tmp = cml_split_space(line);
    //delete last number and (| or !)
    tmp.pop_back();
    string result;
    for (auto const& s : tmp) { result += s + " "; };
    return result;
}
//ex: "|5" 回傳 5 ,default return 0
int is_num_pipe(const string line , bool *e_flag)
{
    vector<string> tmp = cml_split_space(line);
    string cmp = tmp.back();
    if(cmp[0] == '|' || cmp[0] == '!' ){
        if(cmp[0] == '!')
            *e_flag = true;
        cmp.erase(0,1);
        return stoi(cmp);
    }
    return 0;
}
string cml_read_line()
{
    //char *line = NULL;
    string line;
    getline(cin, line);
    return line;
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

vector<string> cml_split_space(string line)
{
    istringstream iss(line);
    vector<std::string> results((istream_iterator<string>(iss)),
                                istream_iterator<string>());
    return results;
}
vector<int> cml_execute(vector<Command> command, Built_in** built_in
        , bool is_num_pipe, bool is_errpr_pipe, vector<int> real_in, int last_out)
{
    //built-in command
    vector<int> nothing;
    nothing.push_back(0);
    nothing.push_back(1);
    if(command.size() == 1) {
        vector<string> tokens = command[0];
        for(int i = 0; i < MAXBUILTIN; i++){
            if(built_in[i]->name == tokens[0]){
                //cout << tokens[0] << endl;
                tokens.erase(tokens.begin());
                int res = 1;
                res = built_in[i]->operation(tokens);
                nothing.push_back(res); //status
                return nothing;
            }
        }
    }
    //common command
    nothing = execute_cmd_seq(command, is_num_pipe, is_errpr_pipe, std::move(real_in), last_out);
    nothing.push_back(1);
    return nothing;
}

vector<int> execute_cmd_seq(vector<Command> command, bool is_num_pipe, bool is_error_pipe,
                            vector<int> real_in, int last_out)
{
    pid_table.clear();
    int fd_in = real_in[0];
    int fd_out = STDOUT_FILENO;
    bool e_flag = false;
    if(last_out != STDOUT_FILENO){
        close(last_out);
        //cout << "close(0):" << last_out << endl;
    }

    int cmd_count = static_cast<int>(command.size());
    int pipeline_count;
    if(is_num_pipe)
        pipeline_count = cmd_count;
    else
        pipeline_count = cmd_count - 1;
    int pipes_fd[MAX_CMD_COUNT][2];
    bool is_not_stdin = real_in[0] != STDIN_FILENO;
    for (int C = 0; C < cmd_count; C++)
    {
        if(is_num_pipe || C != cmd_count - 1){
            if (pipe(pipes_fd[C]) == -1)
            {
                fprintf(stderr, "Error: Unable to create pipe. (%d)\n", C);
                exit(EXIT_FAILURE);
            }
        }
        if(C != 0)
            fd_in = pipes_fd[C-1][0];

        if(!is_num_pipe && C == cmd_count - 1){
            if(real_in[1] == STDOUT_FILENO)
                fd_out = STDOUT_FILENO;
            else
                fd_out = real_in[1];
        }
        else
            fd_out = pipes_fd[C][1];
        //check if it is error pipe
        if(is_num_pipe && C == cmd_count - 1){
            if(is_error_pipe)
                e_flag = true;
        }
        //cout << "in:" << fd_in << " out:"<< fd_out << endl;

        /* use creat_proc fork() Child Process */
        args_execute(command[C], fd_in, fd_out, pipeline_count, pipes_fd, e_flag);

        if(C == 0 && is_not_stdin){
            close(fd_in);
            //cout << "close(1):" << fd_in << endl;
        }

        if(is_num_pipe && C == pipeline_count - 1)
        {
            if(fd_in != STDIN_FILENO) {
                close(fd_in);
                //cout << "C:" << C << " close(2):" << fd_in << endl;
            }
            fd_out = pipes_fd[C][0];
            fd_in = pipes_fd[C][1];
        } else {
            if(fd_in != STDIN_FILENO) {
                close(fd_in);
                //cout << "C:" << C << " close(3):" << fd_in << endl;
            }
            if(fd_out != real_in[1]) {
                close(fd_out);
                //cout << "C:" << C << " close(4):" << fd_out << endl;

            }
        }

    }
    //cout << "return in:" << fd_in << " out:"<< fd_out << endl;
    //後面條件為確保此Non-number不為導入別人pipe退化的number pipe
    if(!is_num_pipe && fd_out == STDOUT_FILENO) {
        for (int &pid : pid_table) {
            int child;
            waitpid(pid, &child, 0);
        }
    }
    vector<int> result = {fd_in,fd_out};
    return result;
}

void args_execute(Command tokens, int fd_in, int fd_out,
                  int pipeline_count, int pipes_fd[][2], bool is_error_pipe)
{
    /*cout << "error pipe? " << is_error_pipe << "\nfork():" ;
    for(auto &c : tokens){
        cout << c << " ";
    }
    cout << endl;*/
    pid_t pid;
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGCHLD, childHandler);
    while((pid = fork()) < 0) {
        usleep (1000);
    }
    pid_table.push_back(pid);
    if(pid == 0){
        //Child process
        vector<char*> tokenc;
        int fd;     //for redirection File
        string path;
        path += tokens[0];

        /* change fd_in & fd_out to stdin & stdout */
        if (fd_in != STDIN_FILENO){
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if(is_error_pipe)
            dup2(fd_out, STDERR_FILENO);
        if (fd_out != STDOUT_FILENO){
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        for(auto i = tokens.begin(); i != tokens.end(); i++){

            if(*i == ">"){
                //implement redirection
                //printf("%s\n",(i+1)->c_str());
                //fflush(stdout);
                if((fd = open((i+1)->c_str(), O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
                    fprintf(stderr, "Can't open - (%s)\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if(dup2(fd, STDOUT_FILENO) < 0){
                    fprintf(stderr, "Can't dup2 - (%s)\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                close(fd);
                break;
            }
            tokenc.emplace_back(const_cast<char *>(i->c_str()));

        }
        //NULL terminate
        tokenc.push_back(nullptr);

        if(execvp(path.c_str(),tokenc.data()) < 0 ){
            //perror("Unknown command");
            fprintf(stderr,"Unknown command: [%s].\n",tokenc[0]);
        }
        exit(EXIT_FAILURE);
    }
}

void sigpipe_handler(int dummy)
{
    fprintf(stderr, "%d: caught a SIGPIPE\n", getpid());
    exit(1);
}

void childHandler(int signo) {
    int status;
    pid_t pid ;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        //do nothing
    }
}