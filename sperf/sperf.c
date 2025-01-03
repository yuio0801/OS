#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/types.h>
#include <fcntl.h>
#include <regex.h>
#include <stdint.h>

struct SC{
    char syname[128];
    double acctime;
};
struct SC sc[2048];
int cmp(const void *a, const void *b) 
{
    const struct SC *sc_A = (const struct SC *)a;
    const struct SC *sc_B = (const struct SC *)b;
    return sc_A->acctime <= sc_B->acctime; 
}

int tot = 0 ;
double tottime = 0;
int print_time = 0;

regex_t regex_syscall, regex_time;
void print()
{
    qsort(sc, tot, sizeof(struct SC), cmp);
    print_time ++;
    printf("Time: %.1lfs\n", (double)0.1 * print_time);
    //printf("\n");
    for(int i = 0; i < (tot > 5 ? 5 : tot); ++i)
    {
        int ratio = (int)(sc[i].acctime * 100 / tottime);
        printf("%s (%d%%)\n", sc[i].syname, ratio);
        //fflush(stdout);
    }
    for(int i = 0; i < 80; ++i)
    {
        putc('\0',stdout);
    }
    fflush(stdout);
    return ;
}
void print_de()
{
    qsort(sc, tot, sizeof(struct SC), cmp);
    print_time ++;
    printf("Time: %.1lfs\n", (double)0.1 * print_time);
    //printf("\n");
    for(int i = 0; i < tot; ++i)
    {
        int ratio = (int)(sc[i].acctime * 100 / tottime);
        printf("%s (%lf)/(%lf) = (%d%%)\n", sc[i].syname,sc[i].acctime, tottime,ratio);
        //fflush(stdout);
    }
    for(int i = 0; i < 80; ++i)
    {
        putc('\0',stdout);
    }
    fflush(stdout);
    return ;
}
void do_parent(int fd, pid_t cpid) 
{
    //regcomp(&regex_syscall, "^[^\\(]+", REG_EXTENDED | REG_NEWLINE);
    regcomp(&regex_syscall, "^[A-z0-9_]+\\(", REG_EXTENDED | REG_NEWLINE);
    regcomp(&regex_time, "<([0-9]+\\.[0-9]+)>$", REG_EXTENDED | REG_NEWLINE);
    // if(fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | O_NONBLOCK) == -1) 
    // {
	// 	perror("fcntl");
	// 	exit(EXIT_FAILURE);
	// }
    clock_t last_time = clock();
    double interval = 0.1;
    int offset = 0;
    tot = 0;
    tottime = 0;
    int BUF_SIZE = 65535;
    char buf[BUF_SIZE];
    char syname[1024];
    while (1) 
    {
        clock_t current_time = clock();
        double diff = (double)(current_time - last_time) / CLOCKS_PER_SEC;
        
        if(diff < interval) continue;
       
        ssize_t len = read(fd, buf + offset, BUF_SIZE - offset - 1);

        if (len == 0)
        {
            int status;
            pid_t result = waitpid(cpid, &status, WNOHANG);
            if (result == 0) 
            {
                continue;
            }
            else
            {
                print();
                print_de();
                return ;
            }
        }
        else if (len > 0) 
        {
            //printf("newline\n");
            offset += len;
            buf[offset] = '\0';
            //printf("buf:%s\n",buf);

            char *tmp = buf, *line = buf;
            
            regmatch_t pmatch;
            for(;*tmp!='\0';++tmp)
            {
                if(*tmp == '\n')
                {
                    char *syscall_s = line, *syscall_e = line;
                    char *time_s = line;
                    *tmp = '\0';
                    //printf("%s\n", line);
                    if(regexec(&regex_syscall, line, 1, &pmatch, 0))
                    {
                        line = tmp + 1;
                        continue;
                    }
                    syscall_s = line + pmatch.rm_so;
                    syscall_e = line + pmatch.rm_eo;
                    if(regexec(&regex_time, line, 1, &pmatch, 0))
                    {
                        line = tmp + 1;
                        continue;
                    }
                    time_s = line + pmatch.rm_so;

                    double tmptime = 0;
                    sscanf(time_s + 1, "%lf", &tmptime);
                    tottime += tmptime;

                    
                    int syname_len = syscall_e - syscall_s - 1;
                    memcpy(syname, syscall_s, syname_len);
                    //syname[syname_len] = '\0';
                    syname[syname_len] = '\0';
                    //printf("get:%s %lf\n", syname, tmptime);
                    int flag = 0;
                    for(int i = 0; i < tot; ++i)
                    {
                        //printf("cmp:%s %s\n",sc[i].syname, syname);
                        if(strcmp(sc[i].syname, syname) == 0)
                        {
                            sc[i].acctime += tmptime;
                            flag = 1;
                            break;
                        }
                    }
                    if(!flag)
                    {
                        strcpy(sc[tot].syname, syname);
                        sc[tot++].acctime = tmptime;
                    }
                    line = tmp + 1;
                }
            }
            // if(!regexec(&regex_syscall, line, 1, &pmatch, 0))
            // {
            //     tmp = line + pmatch.rm_eo;
            // }
            offset = tmp - line;
            if(offset == BUF_SIZE - 1 || offset == BUF_SIZE)
            {
                assert(0);
                exit(1);
                offset = 0;
            }
            else
            {
                memmove(buf, line, offset);
                buf[offset] = '\0';
            }
        }
        print();
            //print_de();
            last_time = current_time;
    }
}
extern char **environ;

int main(int argc, char *argv[], char *envp[]) {
    // for (int i = 0; i < argc; i++) {
    //     assert(argv[i]);
    //     printf("argv[%d] = %s\n", i, argv[i]);
    // }
    assert(!argv[argc]);
    if(argc == 1) 
    {
        assert(0);
    }
    int pipefd[2];

    // Create a pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Fork the current process
    pid_t pid = fork();
    if (pid == -1) 
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    char *PATH = getenv("PATH");
    char *path = strdup(PATH);
    
    if (pid == 0) 
    {
        int dev_null = open("/dev/null", O_WRONLY);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        char fd_path[20];

	    close(pipefd[0]);
        
        char **exec_argv = (char**)malloc(sizeof(char*) * (argc + 5));
        exec_argv[0] = "strace";
        exec_argv[1] = "-T";
        exec_argv[2] = "--string-limit=2";
        exec_argv[3] = "-o";
        for(int i = 1; i <= argc; ++i) 
        { 
            exec_argv[i + 4] = argv[i]; 
        }
        sprintf(fd_path, "/proc/%d/fd/%d", getpid(), pipefd[1]);
	    exec_argv[4] = fd_path;

        // execve("strace",          exec_argv, environ);
        // execve("/bin/strace",     exec_argv, environ);
        // execve("/usr/bin/strace", exec_argv, environ);
        //execvp("strace", exec_argv);
        if(argv[1][0] != '.')
        {
            char *dir = strtok(path, ":");
            while (dir != NULL) 
            {
                // Construct the full path to the file
                char absolute_path[4096];
                snprintf(absolute_path, sizeof(absolute_path), "%s/%s", dir, argv[1]);
                if(access(absolute_path, X_OK) == 0)
                {
                    //dup2(pipefd[1], STDERR_FILENO);
                    // for(int i = 0; i < argc + 3; ++i)
                    // {
                    //     printf("!!!%s ", exec_argv[i]);
                    // }
                    // printf("\n");
                    exec_argv[5] = strdup(absolute_path);
                    break;
                }
                dir = strtok(NULL, ":");
            }
        }
    
        path = strdup(PATH);
	    char *dir = strtok(path, ":");
        while (dir != NULL) 
        {
            // Construct the full path to the file
            char absolute_path[4096];
            snprintf(absolute_path, sizeof(absolute_path), "%s/strace", dir);
            if(access(absolute_path, X_OK) == 0)
            {
                //dup2(pipefd[1], STDERR_FILENO);
                // for(int i = 0; i < argc + 3; ++i)
                // {
                //     printf("!!!%s ", exec_argv[i]);
                // }
                // printf("\n");
                exec_argv[0] = absolute_path;
                execve(absolute_path, exec_argv, environ);
                break;
            }
            dir = strtok(NULL, ":");
        }
        assert(0);
    } 
    else 
    {
        close(pipefd[1]);
        do_parent(pipefd[0], pid);
    }
    return 0;
}
