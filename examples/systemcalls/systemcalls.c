#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int ret = system(cmd);
    return ret==0;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    printf("do_exec start cmd count: %d\n", count);
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        printf("get cmd : %s\n", command[i]);
    }
    printf("do_exec cmd listing done\n");
    fflush(stdout);
    command[count] = NULL;

    va_end(args);
    
/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    pid_t pid;
    // int wstatus;

    pid = fork();
    if (pid < 0){
        printf("Fork failed with %s\n", strerror(errno));
        fflush(stdout); 
        return false;
    }

    if (pid==0){
        for(i=0; i<count; i++){
            printf("cmd: %s\n", command[i]);
        }
        fflush(stdout); 
        int ret = execv( command[0], command ); // the argv should still keep the 1st arg i.e. command[0]
        if (ret == -1){
            perror("child execv failed");
            exit(1); // if return normally the testsuit would be run trice due to the fork
            return false;
        }
    }
    else{
        int status;
        if ( waitpid(pid, &status, 0) == -1 ) {
            perror("waitpid() failed");
            return false;
        }

        // if ( wait(&status) == -1 ) {
        //     perror("wait() failed");
        //     return false;
        // }

        int returned = 0;
        int signum   = 0;

        if ( WIFEXITED(status) ) {
            returned = WEXITSTATUS(status);
            printf("at do_exec, Exited normally with retCode %d\n", returned);
        }
        else if ( WIFSIGNALED(status) ) {
            signum = WTERMSIG(status);
            printf("Exited due to receiving signal %d\n", signum);
        }
        else if ( WIFSTOPPED(status) ) {
            signum = WSTOPSIG(status);
            printf("Stopped due to receiving signal %d\n", signum);
        }
        else {
            printf("Something strange just happened.\n");
        }

        printf("DBG cmd count: %d\n", count);
        for(i=0; i<count; i++)
        {
            printf("DBG cmd : %s\n", command[i]);
        }
        printf("DBG cmd listing done\n");

        
        printf("status %d\n", status);
        printf("Parent receive child %d exit status was %d OR signum %d \n", pid, returned, signum);
        fflush(stdout); 
        return returned == 0;
        
    }

    
    printf("pid %d at do_exec_end\n", pid);
    fflush(stdout); 
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    va_end(args);


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    pid_t pid;
    

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { perror("open"); abort(); }

    pid = fork();
    if (pid < 0){
        printf("Fork failed with %s\n", strerror(errno));
        fflush(stdout); 
        return false;
    }

    if (pid==0){
        for(i=0; i<count; i++){
            printf("cmd: %s\n", command[i]);
        }
        fflush(stdout); 

        if (dup2(fd, 1) < 0) { perror("dup2"); abort(); }
        close(fd);

        int ret = execv( command[0], command);
        if (ret == -1){
            perror("child execv failed");
            return false;
        }
        
    }
    else{
        close(fd);

        int status;
        if ( waitpid(pid, &status, 0) == -1 ) {
            perror("waitpid() failed");
            return false;
        }

        int returned = 0;
        int signum   = 0;

        if ( WIFEXITED(status) ) {
            returned = WEXITSTATUS(status);
            printf("at do_exec_redirect, Exited normally with retCode %d\n", returned);
        }
        else if ( WIFSIGNALED(status) ) {
            signum = WTERMSIG(status);
            printf("Exited due to receiving signal %d\n", signum);
        }
        else if ( WIFSTOPPED(status) ) {
            signum = WSTOPSIG(status);
            printf("Stopped due to receiving signal %d\n", signum);
        }
        else {
            printf("Something strange just happened.\n");
        }

        
        printf("status %d\n", status);
        printf("Parent receive child %d exit status was %d OR signum %d \n", pid, returned, signum);
        fflush(stdout); 
        return returned == 0;
        
    }


    printf("pid %d at do_exec_redirect end\n", pid);
    fflush(stdout); 
    return true;

    
}
