#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

int getopt(int argc, char * const argv[],
                  const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;

#define N 1007
#define M 1000007

struct Node{
  int Pid, PPid;
  char procname[256];
  int son[1007];
  int sumofson;
  int depth;
}node[1007];
int t[1000007], tot = 0;

void mysort(int a[]);
int myisdigit(char s[])
{
  int len = strlen(s);
  for(int i = 0; i < len; ++i)
  {
    if(!isdigit(s[i]))
    {
      return 0;
    }
  }
  return 1;
}

int needpid = 0, needsort = 0, printversion = 0;
char outfile[256] = "";FILE *outfp;

void dfs(int u, int dep)
{
    for(int i = 0; i < dep; ++i)
    {
      printf("  ");
    }
      if(needpid)
      {
        printf("%s(%d)\n", node[u].procname, node[u].Pid);
      }
      else
      {
        printf("%s\n", node[u].procname);
      }
    for(int i = 1; i <= node[u].sumofson; ++i)
    {
      dfs(node[u].son[i], dep + 1);
    }
}

int main(int argc, char *argv[]) {

  DIR *dir;
  struct dirent *entry;

  // 打开目录
  dir = opendir("/proc");

  if (dir == NULL) {
      perror("unable to open /proc");
      exit(EXIT_FAILURE);
  }

  memset(node, 0, sizeof(node));

  // 遍历目录中的文件
  while((entry = readdir(dir)) != NULL) 
  {
      if(entry->d_type == DT_DIR && myisdigit(entry->d_name))
      {
        char filename[256] = "/proc/";
        strcat(filename, entry->d_name);
        strcat(filename, "/status");
        FILE *fp = fopen(filename, "r");
        if(!fp)
        {
          printf("unable to open %s",filename);
          exit(EXIT_FAILURE);
        }
        
        int Pid = atoi(entry->d_name);
        if(!t[Pid])
        {
          t[Pid] = ++tot;
          node[tot].Pid = Pid;
        }

        char buffer[1024], pname[1024], col1[1024], col2[1024];
        
        while(fgets(buffer, sizeof(buffer), fp) != NULL)
        {
          sscanf(buffer, "%s %s", col1, col2);
          if(strcmp(col1, "Name:") == 0)
          {
            strcpy(node[t[Pid]].procname, col2);
          }
          else if(strcmp(col1, "PPid:") == 0)
          {
            int PPid = atoi(col2);
            if(PPid == 0)
            {
              node[t[Pid]].PPid = 0;
            }
            else
            {
              if(!t[PPid])
              {
                t[PPid] = ++tot;
                node[tot].Pid = PPid;
              }
              node[t[Pid]].PPid = PPid;
              node[t[PPid]].son[++node[t[PPid]].sumofson] = t[Pid];
            }

            break;
          }
        }
        fclose(fp);
        }
  }

  

    // 用fscanf, fgets等函数读取
    assert(!argv[argc]);
    for (int i = 0; i < argc; i++) 
    {
        assert(argv[i]);
        if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--show-pids") == 0)
        {
            needpid = 1;
        }
        else if(strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric-sort") == 0)
        {
            needsort = 1;
        }
        else if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0)
        {
            printversion = 1;
        }
    }
    if(printversion)
    {
      fprintf(stderr, "pstree (PSmisc) 23.4\nCopyright (C) 1993-2020 Werner Almesberger and Craig Small\n\nPSmisc comes with ABSOLUTELY NO WARRANTY.\nThis is free software, and you are welcome to redistribute it under\nthe terms of the GNU General Public License.\nFor more information about these matters, see the files named COPYING.\n");
    }
    else
    {
      for(int i = 1;i <= tot; ++i)
      {
          if(node[i].PPid == 0)
          {
              dfs(i, 0);
          }
      }
    }
  
  return 0;
}