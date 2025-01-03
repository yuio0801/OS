#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/wait.h>
#include <math.h>
#include <assert.h>

typedef struct {
    bool check;
    int val;
}Res;

int num_expr = 0;
void *handle;

Res build_express(char *origline, int type) //0-function 1-expression
{
    char txt[128];
    strcpy(txt, origline);
    txt[strlen(origline) - 1] = '\0';
    char tmp_func_name[32];
    char sopath[256];
    if(type == 0)
    {
        char *p = &txt[4];
        for(int i = 0; i < 16; ++i)
        {
            if(p[i] == '(')
            {
                tmp_func_name[i] = '\0';
                break;
            }
            tmp_func_name[i] = p[i];
        }
    }
    else if(type == 1)
    {
        num_expr++;
        sprintf(tmp_func_name, "__expr_wrapper_%d", num_expr);
        char tmp_txt[64];
        strcpy(tmp_txt, txt);
        sprintf(txt, "int %s() {return %s;}", tmp_func_name, tmp_txt);
    }
    strcpy(sopath, "/tmp/");
    strcat(sopath, tmp_func_name);
    strcat(sopath, ".so");

    pid_t pid = fork();
    if (pid == -1) 
    {
        perror("fork");
        return (Res){false, -1};
    } 
    else if (pid == 0) 
    {
        char template[] = "/tmp/exampleXXXXXX";
        int fd;

        fd = mkstemp(template);
        if (fd == -1) {
            perror("mkstemp");
            exit(1);
        }

        dprintf(fd, "%s", txt);
        close(fd);

        // 打印临时文件的路径
        //printf("Created temporary file: %s\n", template);
        if(sizeof(void*) == 4)
        {
            execl("/usr/bin/gcc", "gcc", "-m32", "-x" ,"c", 
            "-shared", "-fPIC", "-Wno-implicit-function-declaration", "-o", sopath, template, NULL);
        }
        else
        {
            execl("/usr/bin/gcc", "gcc", "-m64", "-x" ,"c", 
            "-shared", "-fPIC", "-Wno-implicit-function-declaration", "-o", sopath, template, NULL);
        }
        exit(1);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) 
        { // 如果子进程正常退出
            int exit_status = WEXITSTATUS(status); // 获取子进程的退出状态
            if (exit_status == 0) 
            {
                printf("Compilation .so successful\n");
            } 
            else 
            {
                printf("Compilation .so failed with exit status: %d\n", exit_status);
                return (Res){false, -1};
            }
        }
        
        int (*foo)(void) = NULL;
        char *error;

        // 打开共享库
        handle = dlopen(sopath, RTLD_NOW | RTLD_GLOBAL);
        if (!handle) 
        {
            fprintf(stderr, "%s\n", dlerror());
            return (Res){false, -1};
        }

        dlerror();

        //*(void **) (&foo) = dlsym(handle, tmp_func_name);
        foo = dlsym(handle, tmp_func_name);
        if ((error = dlerror()) != NULL)  
        {
            fprintf(stderr, "%s\n", error);
            dlclose(handle);
            return (Res){false, -1};
        }
        
        
        if(type == 0)
        {
            return (Res){true, 0};
        }
        else
        {
            int result = foo();
            dlclose(handle);
            return (Res){true, result};
        }   
    }
    assert(0);
    return (Res){false, -1};
}

int main(int argc, char *argv[]) {
    static char line[4096];
    while (1) {
        printf("crepl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        // To be implemented.
        
        printf("Got %zu chars.\n", strlen(line));
        //printf("%s\n", line);
        if(line[0] == 'i' && line[1] == 'n' && line[2] == 't')
        {
            Res result = build_express(line, 0);
            if(!result.check)
            {
                printf("fail to build function\n");
            }
            else
            {
                printf("success to build function\n");
            }
        }
        else
        {
            Res result = build_express(line, 1);
            if(!result.check)
            {
                printf("fail to eval expression\n");
            }
            else
            {
                line[strlen(line) - 1] = '\0';
                printf("(%s) == %d\n", line, result.val);
            }
        }
    }
}
