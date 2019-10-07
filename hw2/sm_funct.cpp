//
// Created by ray on 18年11月13日.
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

#include "sm_funct.h"

using namespace std;

vector<pid_t> pid_table;

static void client_handler(int sig);
static int sem_get_access();
static int sem_release_access();
static int shmid = 0, semid = 0, uid = 0;
static string orig_line;
static Client *client_set;

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

void cml_loop(struct sockaddr_storage remoteaddr)
{
    // init shared memory (client_set) & semaphore
    initialize();

    // client signal handlers
    signal(SIGUSR1, client_handler);	// receive messages from others
    signal(SIGINT , client_handler);
    signal(SIGQUIT, client_handler);
    signal(SIGTERM, client_handler);

    // welcome message
    welcome();

    // login message
    if (login(remoteaddr) < 0) {
        shmdt (client_set);
        return;
    }

    vector<Num_pipe> num_pipe_list;
    int status = 1;
    while(status)
    {
        int n_flag, up_flag = 0, fup_flag = 0, fup_id = 0;
        int old_stdin, old_stdout;
        bool e_flag = false;
        vector<int> fd_in_out = { STDIN_FILENO, STDOUT_FILENO};
        int last_out = STDOUT_FILENO;
        string line;
        vector<string> tokens;
        vector<Command> command;

        printf("%% ");
        fflush(stdout);

        line = cml_read_line();
        orig_line = line;


        //size = 1 represent line only has '\0'
        if(line.size()!=1) {
            // 判斷各種pipe
            // numb pipe input
            n_flag = is_num_pipe(line, &e_flag);

            //not null command
            for (auto P = num_pipe_list.begin(); P != num_pipe_list.end();) {
                if (P->sub_counter()) {
                    if (P->is_counter_zero()) {
                        fd_in_out[0] = P->get_fd_in();
                        last_out = P->get_fd_out();
                        num_pipe_list.erase(P);
                    } else
                        P++;
                } else {
                    P++;
                }
            }

            // from user pipe
            fup_flag = from_user_pipe(line, &fup_id);
            if( fup_flag  < 0)
                continue;
            else if(fup_flag){
                // 找到此sender的pipe資訊
                old_stdin = dup(STDIN_FILENO);      // 暫存舊的in到old_stdin
                dup2(fup_flag, STDIN_FILENO);        // in接到新的input
            }

            // user pipe input
            up_flag = to_user_pipe(line);
            if (up_flag < 0){
                continue;
            } else if (up_flag > 0){
                old_stdout = dup(STDOUT_FILENO);
                dup2(up_flag, STDOUT_FILENO);
            }

            for (auto &p : num_pipe_list) {
                //p.debugger();
                if (n_flag && n_flag == p.get_fd_counter()) {
                    //進同一個pipe
                    fd_in_out[1] = p.get_fd_out();
                    //flag清空，此command蛻變為一般command僅output進pipe(非stdout)
                    n_flag = 0;
                    line = dele_num_pipe(line);
                    //cout << line << endl ;
                    break;
                }
            }
            // 若不是number pipe直接執行
            if (!n_flag) {
                tokens = cml_split_line(line, '|');
                //cout << "full cml:" << line << endl;
                for (auto const &a : tokens) {
                    command.push_back(cml_split_space(a));
                }

                status = cml_execute(command, false, e_flag, fd_in_out, last_out)[2];
            } else {
                // number pipe先暫存在command_list中
                tokens = cml_split_line(dele_num_pipe(line), '|');
                for (auto const &a : tokens) {
                    command.push_back(cml_split_space(a));
                }
                vector<int> debugger = cml_execute(command, true, e_flag, fd_in_out, last_out);
                status = debugger[2];
                Num_pipe num_pipe(n_flag, debugger[1], debugger[0]);
                num_pipe_list.push_back(num_pipe);
            }
        }

        // user pipe receiver (close fd)
        if (fup_flag) {
            dup2 (old_stdin, STDIN_FILENO);
            close(old_stdin);
            close(fup_flag);
            char fname[20] = "";
            sprintf(fname, "user_pipe/%d->%d", fup_id+1, uid+1);
            remove(fname);
        }
        // user pipe sender (close fd)
        if (up_flag){
            dup2 (old_stdout, STDOUT_FILENO);
            close(old_stdout);
            close(up_flag);
        }
    }

    // logout message
    logout();

    // detach shared memory
    shmdt (client_set);
}

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
string cml_read_line()
{
    //char *line = NULL;
    string line;
    getline(cin, line);
    return line;
}
vector<int> cml_execute(vector<Command> command, bool is_num_pipe,
        bool is_errpr_pipe, vector<int> real_in, int last_out)
{
    //built-in command
    vector<int> nothing;
    nothing.push_back(0);
    nothing.push_back(1);
    if(command.size() == 1) {
        vector<string> tokens = command[0];
        int res = 1;
        /* execute the command accordingly */
        if (tokens[0] == "exit") {
            if(real_in[0] != STDIN_FILENO){
                close(real_in[0]);
            }
            if(last_out != STDOUT_FILENO) {
                close(last_out);
            }
            res = 0;
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "printenv") {
            res = printenv (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "setenv") {
            res = setupenv (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "who") {
            res = who (tokens.size());
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "name") {
            res = name (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "tell") {
            res = tell (tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
        } else if (tokens[0] == "yell") {
            res = yell(tokens.size(), tokens);
            nothing.push_back(res); //status
            return nothing;
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

void broadcast (char *msg)
{
    for(int i = 0; i < MAX_CLIENT; i++)
    {
        // for all available client
        if (client_set[i].id > 0) {
            for (int j = 0; j < MAX_MSG_TMP; j++) {
                //----- SEMAPHORE GET ACCESS -----
                if (!sem_get_access())
                    exit(EXIT_FAILURE);

                // write into msg_tmp
                if (client_set[i].msg[uid][j][0] == 0) {
                    strncpy (client_set[i].msg[uid][j], msg, MAX_MSG_SIZE + 1);

                    //----- SEMAPHORE RELEASE ACCESS -----
                    if (!sem_release_access())
                        exit(EXIT_FAILURE);

                    kill (client_set[i].pid, SIGUSR1);
                    break;
                }

                //----- SEMAPHORE RELEASE ACCESS -----
                if (!sem_release_access())
                    exit(EXIT_FAILURE);
            }
        }
    }
}

void initialize()
{
    // get the shared memory for client
    if ((shmid = shmget (SHMKEY, MAX_CLIENT * sizeof (Client), SHMFLAG)) < 0) {
        perror("shmget");
        exit (1);
    }

    // attach the allocated shared memory
    if ((client_set = (Client *) shmat (shmid, nullptr, 0)) == (Client *) -1) {
        perror("shmat");
        exit (1);
    }

    // get the semaphore for client
    if ((semid =  semget((key_t)SEMKEY, 1, 0666)) < 0){
        perror("semget");
        exit(1);
    }

}

int login(struct sockaddr_storage remoteaddr)
{
    char	msg[MAX_MSG_SIZE + 1];

    for (int i = 0; i < MAX_CLIENT; i++)
    {
        //----- SEMAPHORE GET ACCESS -----
        if (!sem_get_access())
            exit(EXIT_FAILURE);
        if (client_set[i].id == 0) {
            client_set[i].id = i + 1;
            client_set[i].pid = getpid ();
            strncpy(client_set[i].name,"(no name)",MAX_NAME);
            // CGILAB/511
            //inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr *) &remoteaddr),
            //          client_set[i].addr, INET6_ADDRSTRLEN);
            strcpy(client_set[i].addr,"CGILAB");
            //client_set[i].port = ntohs(get_in_port((struct sockaddr *) &remoteaddr));
            client_set[i].port = 511;

            // broadcast login  *** User '<username>' entered from <IP>/<port>. ***
            snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s/%d. ***\n",
                    client_set[i].name, client_set[i].addr, client_set[i].port);
            uid = i;

            //----- SEMAPHORE RELEASE ACCESS -----
            if (!sem_release_access())
                exit(EXIT_FAILURE);

            broadcast(msg);
            //printf("%s",msg);

            return i;
        }
        //----- SEMAPHORE RELEASE ACCESS -----
        if (!sem_release_access())
            exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Sorry! The server is full.\n");
    return -1;
}

int logout()
{
    char	msg[MAX_MSG_SIZE + 1];
    // erase user pipe file
    for (int i = 0; i < MAX_CLIENT; i++) {
        char sfile[20] = "";
        char rfile[20] = "";
        sprintf(sfile, "user_pipe/%d->%d", uid+1, i+1);
        sprintf(rfile, "user_pipe/%d->%d", i+1, uid+1);
        remove(sfile);
        remove(rfile);
    }
    //----- SEMAPHORE GET ACCESS -----
    if (!sem_get_access())
        exit(EXIT_FAILURE);

    // broadcast logout  *** User '<username>' left. ***
    snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' left. ***\n",client_set[uid].name);

    //----- SEMAPHORE RELEASE ACCESS -----
    if (!sem_release_access())
        exit(EXIT_FAILURE);


    broadcast(msg);

    //----- SEMAPHORE GET ACCESS -----
    if (!sem_get_access())
        exit(EXIT_FAILURE);

    memset (&client_set[uid], 0, sizeof(Client));

    //----- SEMAPHORE RELEASE ACCESS -----
    if (!sem_release_access())
        exit(EXIT_FAILURE);


}

//built_in
int name (int argc, vector<string> argv)
{
    char	msg[MAX_MSG_SIZE + 1];

    if (argc == 2) {
        //----- SEMAPHORE GET ACCESS -----
        if (!sem_get_access())
            exit(EXIT_FAILURE);


        for (int i = 0; i < MAX_CLIENT; i++) {
            if (client_set[i].name == argv[1]) {
                // 'name' already exists changing will be failed
                printf("*** User '%s' already exists. ***\n", client_set[i].name);
                //----- SEMAPHORE RELEASE ACCESS -----
                if (!sem_release_access())
                    exit(EXIT_FAILURE);
                return 1;
            }
        }
        strncpy (client_set[uid].name, argv[1].c_str(), MAX_NAME + 1);
        // broadcast change *** User from <IP>/<port> is named '<newname>'. ***
        snprintf (msg, MAX_MSG_SIZE + 1, "*** User from %s/%d is named '%s'. ***\n"
                , client_set[uid].addr, client_set[uid].port, client_set[uid].name);

        //----- SEMAPHORE RELEASE ACCESS -----
        if (!sem_release_access())
            exit(EXIT_FAILURE);

        broadcast(msg);

    } else
        fputs ("Usage: name <newname>\n", stderr);
    return 1;
}

int yell (int argc, vector<string> argv)
{
    char	msg[MAX_MSG_SIZE + 1];
    // broadcast yell *** <sender’s name> yelled ***: <message>
    if(argc > 1){
        //----- SEMAPHORE GET ACCESS -----
        if (!sem_get_access())
            exit(EXIT_FAILURE);


        sprintf(msg, "*** %s yelled ***: ", client_set[uid].name);
        int counter;
        for(counter=0; counter<orig_line.length(); counter++)
        {

        }
        for(int i = 1; i < argc; i++)
        {
            strcat(msg, argv[i].c_str());
            if(i != argc -1)
                strcat(msg, " ");
            else
                strcat(msg, "\n");
        }

        //----- SEMAPHORE RELEASE ACCESS -----
        if (!sem_release_access())
            exit(EXIT_FAILURE);

        broadcast (msg);

    } else{
        fputs ("Usage: yell <message>\n", stderr);
    }
    return 1;
}

int tell (int argc, vector<string> argv)
{
    char	msg[MAX_MSG_SIZE + 1];

    if (argc >=3) {

        int rid     = stoi(argv[1]);
        int r_index = rid -1;
        if (r_index >= 0 && r_index < 30 && client_set[r_index].id) {
            // *** <sender’s name> told you ***: <message>
            sprintf(msg, "*** %s told you ***: ", client_set[uid].name);
            for(int i = 2; i < argc; i++)
            {
                strcat(msg, argv[i].c_str());
                if(i != argc -1)
                    strcat(msg, " ");
                else
                    strcat(msg, "\n");
            }
            for (int i = 0; i < MAX_MSG_TMP; ++i) {
                if (client_set[r_index].msg[uid][i][0] == 0) {
                    strncpy (client_set[r_index].msg[uid][i], msg, MAX_MSG_SIZE + 1);
                    kill (client_set[r_index].pid, SIGUSR1);
                    break;
                }
            }
        } else {
            // receiver id not find *** Error: user #5 does not exist yet. ***
            printf("*** Error: user #%d does not exist yet. ***\n", rid);
        }

    } else{
        fputs ("Usage: tell <user id> <message>\n", stderr);
    }
    return 1;
}

int who (int argc)
{
    char	msg[MAX_MSG_SIZE + 1];
    if(argc > 1)
        fputs ("Usage: who\n", stderr);
    else {
        //----- SEMAPHORE GET ACCESS -----
        if (!sem_get_access())
            exit(EXIT_FAILURE);


        snprintf(msg, MAX_MSG_SIZE + 1, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
        printf("%s", msg);
        for (int i = 0; i < MAX_CLIENT; i++) {
            if (client_set[i].id != 0) {
                if (strlen(client_set[i].name) < 8)
                    snprintf(msg, MAX_MSG_SIZE + 1, "%d\t%s\t\t%s/%d", client_set[i].id, client_set[i].name,
                             client_set[i].addr, client_set[i].port);
                else
                    snprintf(msg, MAX_MSG_SIZE + 1, "%d\t%s\t%s/%d", client_set[i].id, client_set[i].name,
                             client_set[i].addr, client_set[i].port);
                if (i == uid)
                    strcat(msg, "\t\t<-me\n");
                else
                    strcat(msg, "\n");
                printf("%s", msg);
            }
        }

        //----- SEMAPHORE RELEASE ACCESS -----
        if (!sem_release_access())
            exit(EXIT_FAILURE);
    }
    return 1;
}

//ex: ">5" 回傳 (id)->5的fd (number),default return 0, <0 表錯誤
int to_user_pipe(string &line)
{
    char	msg[MAX_MSG_SIZE + 1];
    vector<string> tmp, orig;
    tmp = cml_split_space(line);
    orig = cml_split_space(orig_line);
    //auto P = client.num_pipe_list.begin(); P != client.num_pipe_list.end();
    for(auto i = tmp.begin(); i != tmp.end();){
        if((*i)[0] == '>' && i->size() > 1){
            string tmp_id;
            tmp_id = *i;
            tmp_id.erase(0,1);
            int rid = stoi(tmp_id) -1;
            int s_index = uid;
            int r_index = rid;

            //----- SEMAPHORE GET ACCESS -----
            if (!sem_get_access())
                exit(EXIT_FAILURE);

            // 找不到id
            if (r_index >= 0 && r_index < MAX_CLIENT && client_set[r_index].id) {
                char fname[20] = "";
                sprintf(fname, "user_pipe/%d->%d", uid+1, rid+1);
                int fd = open(fname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR) ;
                // 檔案存在(已經傳過) *** Error: the pipe #<sender_id>->#<receiver_id> already exists. ***
                if (fd < 0){
                    printf("*** Error: the pipe #%d->#%d already exists. ***\n", uid+1, rid+1);
                    //----- SEMAPHORE RELEASE ACCESS -----
                    if (!sem_release_access())
                        exit(EXIT_FAILURE);
                    return -1;
                }

                // *** <sender_name> (#<sender_id>) just piped '<command>' to <receiver_name> (#<receiver_id>) ***
                sprintf(msg, "*** %s (#%d) just piped '", client_set[s_index].name, client_set[s_index].id);
                for (auto s = orig.begin(); s != orig.end(); s++)
                {
                    strcat(msg, s->c_str());
                    if(s != orig.end()-1)
                        strcat(msg, " ");;
                }
                sprintf(msg + strlen(msg),"' to %s (#%d) ***\n", client_set[r_index].name, client_set[r_index].id);
                //----- SEMAPHORE RELEASE ACCESS -----
                if (!sem_release_access())
                    exit(EXIT_FAILURE);

                broadcast(msg);
                tmp.erase(i);
                line.clear();
                for (auto const& s : tmp) { line += s + " "; };
                return fd;
            }
            else{
                // receiver id not find
                printf("*** Error: user #%d does not exist yet. ***\n", rid+1);
                //----- SEMAPHORE RELEASE ACCESS -----
                if (!sem_release_access())
                    exit(EXIT_FAILURE);
                return -1;
            }

        }
        else
            i++;
    }
    return 0;

}

//ex: "<5" 回傳 5>(id) 的fd (number),default return 0, <0 表錯誤
int from_user_pipe(string &line, int *fup_id)
{
    char	msg[MAX_MSG_SIZE + 1];;
    vector<string> tmp, orig;
    tmp = cml_split_space(line);
    orig = cml_split_space(orig_line);
    //auto P = client.num_pipe_list.begin(); P != client.num_pipe_list.end();
    for(auto i = tmp.begin(); i != tmp.end();){
        if((*i)[0] == '<' && i->size() > 1){
            string tmp_id;
            tmp_id = *i;
            tmp_id.erase(0,1);
            int sid = stoi(tmp_id) -1;
            *fup_id = sid;
            int s_index = sid;
            int r_index = uid;

            //----- SEMAPHORE GET ACCESS -----
            if (!sem_get_access())
                exit(EXIT_FAILURE);

            // 找不到sender id
            if (s_index >= 0 && s_index < MAX_CLIENT && client_set[s_index].id) {
                char fname[20] = "";
                sprintf(fname, "user_pipe/%d->%d", sid+1, uid+1);
                int fd = open(fname, O_RDWR, S_IRUSR | S_IWUSR) ;
                // sender沒有傳過 *** Error: the pipe #<sender_id>->#<receiver_id> does not exist yet. ***
                if (fd < 0){
                    printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n", sid+1, uid+1);
                    //----- SEMAPHORE RELEASE ACCESS -----
                    if (!sem_release_access())
                        exit(EXIT_FAILURE);
                    return -1;
                }

                // 找到sender *** <receiver_name> (#<receiver_id>) just received from <sender_name> (#<sender_id>) by '<command>' ***
                sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '"
                        , client_set[r_index].name, client_set[r_index].id,  client_set[s_index].name, client_set[s_index].id);
                for (auto s = orig.begin(); s != orig.end(); s++)
                {
                    strcat(msg, s->c_str());
                    if(s != orig.end()-1)
                        strcat(msg, " ");;
                }
                sprintf(msg + strlen(msg),"' ***\n");
                //----- SEMAPHORE RELEASE ACCESS -----
                if (!sem_release_access())
                    exit(EXIT_FAILURE);

                broadcast(msg);
                tmp.erase(i);
                line.clear();
                for (auto const& s : tmp) { line += s + " "; };
                return fd;

            }
            else{
                // sender id not find
                printf("*** Error: user #%d does not exist yet. ***\n", sid+1);
                //----- SEMAPHORE RELEASE ACCESS -----
                if (!sem_release_access())
                    exit(EXIT_FAILURE);
                return -1;
            }
        }
        else
            i++;
    }
    return 0;
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
void client_handler(int sig)
{
    // receive messages signo
    if (sig == SIGUSR1) {
        //----- SEMAPHORE GET ACCESS -----
        if (!sem_get_access())
            exit(EXIT_FAILURE);
        for (int i = 0; i < MAX_CLIENT; ++i)
        {
            for (int j = 0; j < MAX_MSG_TMP; ++j)
            {
                // have msg, print out
                if (client_set[uid].msg[i][j][0] != 0) {
                    write (STDOUT_FILENO, client_set[uid].msg[i][j], strlen (client_set[uid].msg[i][j]));
                    // clear msg
                    memset (client_set[uid].msg[i][j], 0, MAX_MSG_SIZE);
                }
            }
        }
        //----- SEMAPHORE RELEASE ACCESS -----
        if (!sem_release_access())
            exit(EXIT_FAILURE);
    }
    else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
        logout();
        close (STDIN_FILENO );
        close (STDOUT_FILENO);
        close (STDERR_FILENO);

        shmdt (client_set);
    }

    signal (sig, client_handler);
}
int sem_get_access()
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = -1; /* P() */
    sem_b.sem_flg = SEM_UNDO;
    if (semop(semid, &sem_b, 1) == -1)		//Wait until free
    {
        fprintf(stderr, "semaphore1_get_access failed\n");
        return 0;
    }
    return 1;
}
int sem_release_access()
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = 1; /* V() */
    sem_b.sem_flg = SEM_UNDO;
    if (semop(semid, &sem_b, 1) == -1)
    {
        fprintf(stderr, "semaphore1_release_access failed\n");
        return 0;
    }
    return 1;
}