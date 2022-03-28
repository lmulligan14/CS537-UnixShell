#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>

//Check if string is only whitespace
int is_empty(const char *s) 
{
  while (*s != '\0') 
  {
    if (!isspace((unsigned char)*s))
      return 0;
    s++;
  }
  return 1;
}

int main(int argc, char *argv[])
{
    int MODE;
    const int INTER = 0;
    const int BATCH = 1;
    const int SIZE = 15;
    char *cmd = NULL;
    size_t len = 1;
    char *cmd_argv[SIZE];
    char *path[SIZE];
    FILE *file;
    char error_message[30] = "An error has occurred\n";

    //Allocate memory for path[]
    for (int i = 0; i < SIZE; i++)
    {
        path[i] = malloc(20*sizeof(char));
        if (path[i] == NULL)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(0);
        }
    }
    strcpy(path[0], "/bin");

    //determine mode
    if (argc > 1)
    {
        MODE = BATCH;
        file = fopen(argv[1], "r"); 
        if (file == NULL || argv[2] != NULL)
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
    }
    else
        MODE = INTER;

    while (1)
    {   begin:
        if (MODE == INTER)
        {
            //prompt for and get input
            printf("wish> ");
            getline(&cmd, &len, stdin);
            if (!is_empty(cmd))
                cmd[strcspn(cmd, "\n")] = 0;
            else
                goto begin;
        }
        else
        {   
            //read line from file
            if (getline(&cmd, &len, file) == -1)
                exit(0);
            
            if (!is_empty(cmd))
                cmd[strcspn(cmd, "\n")] = 0;
            else
                goto begin;
        }

        //seperate args  
        char *tok = strtok(cmd, " \t");
        for (int i = 0; i < SIZE; i++)
        {   
            cmd_argv[i] = tok;
            tok = strtok(NULL, " \t");
        }

        //check for exit command
        if (strcmp(cmd_argv[0], "exit") == 0)
        {
            if (cmd_argv[1] != NULL)
                write(STDERR_FILENO, error_message, strlen(error_message));
            else
            {
                if (cmd != NULL)
                    free(cmd);
                if (file != NULL)
                    fclose(file);
                for (int i = 0; i < SIZE; i++)
                {
                    free(path[i]);
                }
                exit(0);
            }
        }
        //check for path command
        else if (strcmp(cmd_argv[0], "path") == 0)
        {
            for (int j = 0; j < SIZE; j++)
            {
                if (cmd_argv[j+1] != NULL)
                    strcpy(path[j], cmd_argv[j+1]);
                else
                    path[j] = NULL;
                //path[j] = cmd_argv[j+1];
            }
        }
        //check for cd command
        else if (strcmp(cmd_argv[0], "cd") == 0)
        {
            if (cmd_argv[1] == NULL || cmd_argv[2] != NULL)
                write(STDERR_FILENO, error_message, strlen(error_message));
            else
            {
                if (chdir(cmd_argv[1]) == -1)
                    write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
        //non built-in commands
        else
        {
            char *args[SIZE];
            char *outfile = NULL;
            char ch;
            int $loop = -1;
            int valid = 1;

            //check for loop command and copy argument array
            int loopnum = 1;
            if (strcmp(cmd_argv[0], "loop") == 0)
            {
                if (cmd_argv[1] == NULL || sscanf(cmd_argv[1], "%i%c", &loopnum, &ch) != 1 || loopnum <= 0)
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    goto begin;
                }
                
                for (int i = 0; i < SIZE-2; i++)
                {
                    if (cmd_argv[i+2] != NULL && strcmp(cmd_argv[i+2], ">") == 0)
                    {
                        outfile = cmd_argv[i+3];
                        if (outfile == NULL || cmd_argv[i+4] != NULL)
                            valid = 0;
                        break;
                    }
                    else if (cmd_argv[i+2] != NULL && strchr(cmd_argv[i+2], '>') != NULL)
                    {
                        args[i] = strsep(&cmd_argv[i+2], ">");
                        outfile = strsep(&cmd_argv[i+2], ">");
                        if (outfile == NULL)
                            valid = 0;
                        break;
                    }
                    else if (cmd_argv[i+2] != NULL && strcmp(cmd_argv[i+2], "$loop") == 0)
                        $loop = i;
                    args[i] = cmd_argv[i+2];
                }
            }
            else
            {
                for (int i = 0; i < SIZE; i++)
                {
                    if (cmd_argv[i] != NULL && strcmp(cmd_argv[i], ">") == 0)
                    {
                        outfile = cmd_argv[i+1];
                        if (outfile == NULL || cmd_argv[i+2] != NULL)
                            valid = 0;
                        break;
                    }
                    else if (cmd_argv[i] != NULL && strchr(cmd_argv[i], '>') != NULL)
                    {
                        args[i] = strsep(&cmd_argv[i], ">");
                        outfile = strsep(&cmd_argv[i], ">");
                        if (outfile == NULL)
                            valid = 0;
                        break;
                    }
                    args[i] = cmd_argv[i];
                }
            }

            //perform command
            for (int j = 1; j <= loopnum; j++)
            {
                //fork
                int rc = fork();

                //if child, respond to commands
                if (rc == 0)
                {   
                    char *pathname;

                    if ($loop != -1)
                        sprintf(args[$loop], "%d", j);

                    for (int i = 0; i < SIZE; i++)
                    {
                        if (path[i] != NULL)
                        {
                            pathname = path[i];
                            strcat(pathname, "/");
                            strcat(pathname, args[0]);

                            if (access(pathname, X_OK) == 0)
                            {
                                //redirect output to file
                                if (valid == 0)
                                    break;
                                if (outfile != NULL)
                                {
                                    int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                                    dup2(fd, STDOUT_FILENO);
                                    dup2(fd, STDERR_FILENO);
                                }
                                //execute command
                                if (execv(pathname, args) == -1)
                                {
                                    write(STDERR_FILENO, error_message, strlen(error_message));
                                    exit(0);
                                }
                            }
                        }
                    }
                    
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(0);
                }
                //if parent, do nothing
                else if (rc > 0)
                {
                    (void) wait(NULL);
                }
                else
                {
                    //failure
                }
            }
        }
    }
}