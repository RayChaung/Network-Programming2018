#include<string.h>
#include<unistd.h>      //unix std包含UNIX系统服務的函數， ex:read、write、getpid
#include<sys/types.h>   //是Unix/Linux系统的基本系统數據類型的header文件， ex:size_t、time_t、pid_t
#include<sys/wait.h>    //wait、waitid、waitpid...
#include "function.h"

using namespace std;

int main(int argc, char **argv) {

    //config files
    putenv(const_cast<char *>("PATH=bin:."));
    //command loop
    cml_loop();
    exit(EXIT_SUCCESS);
}