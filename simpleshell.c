#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h> // isspace()
#include <stdbool.h> // bool
#include <sys/wait.h> // fork
#include <fcntl.h> // open

/* Macro definitions */
#define CMDLINE_MAX 512
#define MAX_ARG 16
#define MAX_CHAR_ARG 32
#define REDIRECT_DEFAULT 100
#define MAX_PATH 256
#define MAX_ENV_VAR 26


typedef struct Cmds_struct {

    int num_cmd; // how many commands to be performed, default is 1
    int cmd1; // command 1 index, assume to be 0 if not changed
    int cmd2; // command 2 index, the location of the next command
    int cmd3; // command 3 index
    int cmd4; // command 4 index
    int end_of_cmd; // where the last NULL element is
    char **list_cmd; // the data structure
    int redirect; // this will be 100 if there's no redirection
    bool err_redirect; // if error redirection enabled for output redirection
    bool err_redirect1; // error redirection for each pipe
    bool err_redirect2;
    bool err_redirect3;

} Cmds;

/* Prints various error message to STDERR */
void errorMessage(char *message) {
    fprintf(stderr, "%s", message);
}

/* Prints the completion message to STDERR according to the number of commands */
void completionMessage(const char *cmd, int errCode1, int errCode2, int errCode3, int errCode4, int num_cmd) {
    switch (num_cmd) {
        case 1:
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd,
                    errCode1);
            break;
        case 2:
            fprintf(stderr, "+ completed '%s' [%d][%d]\n", cmd,
                    errCode1, errCode2);
            break;
        case 3:
            fprintf(stderr, "+ completed '%s' [%d][%d][%d]\n", cmd,
                    errCode1, errCode2,
                    errCode3);
            break;
        case 4:
            fprintf(stderr, "+ completed '%s' [%d][%d][%d][%d]\n", cmd,
                    errCode1, errCode2,
                    errCode3, errCode4);
            break;
    }
}

// Function that uses the ASCII of the variable and set as the index in the map
int key_index(char letter) {
    return ((int) (letter)) - ((int) 'a');
}

// Function that sets the environment variable to be the desired string
bool set(Cmds *commands, char **env_variables) {

    // confirm that the variable is valid
    if ((*commands).list_cmd[1][1] == '\0' &&
        !islower((int) ((*commands).list_cmd[1][1])) &&
        !isalpha((int) ((*commands).list_cmd[1][1]))) {
        // find index based on letter
        int index = key_index(*((*commands).list_cmd[1]));

        // set key at the index to equal the letter
        int valIndex = 0;
        while (*(*((*commands).list_cmd + 2) + valIndex) != '\0') {
            *(*(env_variables + index) + valIndex) = *(*((*commands).list_cmd + 2) + valIndex);
            valIndex++;
        }
        return true;

    } else {
        // invalid name
        errorMessage("Error: invalid variable name\n");
        return false;
    }

}

void error_redirection_pipe(Cmds *commands) {

    switch ((*commands).num_cmd) {
        case 2:
            (*commands).err_redirect1 = true;
            break;
        case 3:
            (*commands).err_redirect2 = true;
            break;
        case 4:
            (*commands).err_redirect3 = true;
            break;
    }
}

// Parsing the command line into a list
bool toList(const char *cmd, Cmds *commands, char **env_variables) {

    // Initializing counter variables
    int iArg = 0;
    int iStr = 0;
    int i = 0;
    (*commands).num_cmd = 1; // defaults to 1
    (*commands).cmd1 = 0; // defaults to the first element
    (*commands).redirect = REDIRECT_DEFAULT;
    while (cmd[i] != '\0' && iStr != MAX_CHAR_ARG) {

        if (isspace((int) cmd[i]) == 0) {
            /* Pipeline check */
            if (cmd[i] == '|' && (*commands).redirect == REDIRECT_DEFAULT) {
                (*commands).num_cmd++; // number of command counter
                // Mark the end of a command as NULL
                *((*commands).list_cmd + iArg) = NULL;

                // Check for error redirection
                if (cmd[i + 1] == '&') {
                    error_redirection_pipe(commands);
                    i++;
                }

                // Chooses command to update
                switch ((*commands).num_cmd) {
                    case 2:
                        (*commands).cmd2 = iArg + 1;
                        break;
                    case 3:
                        (*commands).cmd3 = iArg + 1;
                        break;
                    case 4:
                        (*commands).cmd4 = iArg + 1;
                        break;
                    default:
                        errorMessage("Error: too much piping\n");
                        return false;
                }
            } else if (cmd[i] == '|' && (*commands).redirect != REDIRECT_DEFAULT) {

                errorMessage("Error: mislocated output redirection");
                return false;

                /* Output redirection check */
            } else if (cmd[i] == '>') {

                if (cmd[i + 1] == '&') {
                    (*commands).err_redirect = true;
                    (*commands).redirect = iArg + 2;
                } else {
                    (*commands).redirect = iArg + 1;
                }
                *((*commands).list_cmd + iArg) = NULL;

                /* Environment variable check */
            } else if (cmd[i] == '$') {

                // Find index of where letter is located in the key value map
                if (cmd[i + 2] == '\0') {
                    int index = key_index(cmd[i + 1]);

                    if (strcmp(env_variables[index], "\0")) {
                        int valIndex = 0;
                        while (env_variables[index][valIndex] != '\0') {
                            *(*(((*commands).list_cmd) + iArg) + valIndex) = env_variables[index][valIndex];
                            valIndex++;
                        }
                        i++;
                    }
                } else {
                    errorMessage("Error: invalid variable name\n");
                    return false;
                }

            } else {
                /* List update */
                *(*(((*commands).list_cmd) + iArg) + iStr) = cmd[i];
            }

            iStr++;
        }
        if ((isspace((int) cmd[i]) != 0 || cmd[i] == '|' || cmd[i] == '>') && isspace((int) cmd[i + 1]) == 0) {
            // If the current is a space but the next one is not a space so we found a new argument
            if (*((*commands).list_cmd + iArg) != NULL) {
                *(*(((*commands).list_cmd) + iArg) + iStr) = '\0';
            }
            // Make each argument into a string by terminating with null
            iArg++;
            iStr = 0;
        }
        if (iArg == MAX_ARG) {
            errorMessage("Error: too many process arguments\n");
            return false;
        }
        i++;
    }

    if (cmd[i] == '\0') {
        iArg++;
        *((*commands).list_cmd + iArg) = NULL;
        (*commands).end_of_cmd = iArg;
        // So that the command would terminate with a NULL
    }

    return true;
}


char **createEmptyList(int sizeList, int sizeItem) {
    char **args = (char **) malloc(sizeList * sizeof(char *));
    // 16 items and a NULL in the end
    for (int item = 0; item < sizeList; item++) {
        *(args + item) = (char *) malloc(sizeItem * sizeof(char));
        // 32 chars in one string and an extra space for \0
        if (*(args + item) == NULL) {
            perror("malloc");
        }
        for (int chars = 0; chars < sizeItem; chars++) {
            *(*(args + item) + chars) = '\0';
            // Initialize everything to \0
        }
    }
    return args;
}

// Prevent memory leaks
void delete_list(char ***list, int size) {
    for (int i = 0; i < size; ++i) {
        free((*list)[i]);
    }
    free(*list);
    *list = NULL;
}

void redirect(const Cmds *commands) {
    if ((*commands).redirect != REDIRECT_DEFAULT) {
        int fd;

        fd = open((*commands).list_cmd[(*commands).redirect],
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);

        if ((*commands).err_redirect)
            dup2(fd, STDERR_FILENO);

        dup2(fd, STDOUT_FILENO);

        close(fd);
    }
}


bool reg_commands(const Cmds *commands, const char *cmd) {
    pid_t pid = fork();

    if (pid == 0) {
        /* Child */
        redirect(commands);
        execvp((*commands).list_cmd[0], (*commands).list_cmd);

        errorMessage("Error: command not found\n");

        exit(1);

    } else if (pid > 0) {
        /* Parent */
        int status;
        // wait for child to finish and get the status
        waitpid(pid, &status, 0);

        completionMessage(cmd, WEXITSTATUS(status), 0, 0, 0, 1);
    }

    return true;
}

bool pipeline(Cmds *commands, const char *cmd) {
    pid_t p1, p2, p3, p4;
    int status1, status2, status3, status4;
    int fd1[2];
    int fd2[2];
    int fd3[2];

    pipe(fd1);

    if (!(p1 = fork())) {

        close(fd1[0]);

        if ((*commands).err_redirect1) {
            dup2(fd1[1], STDERR_FILENO);
        }

        dup2(fd1[1], STDOUT_FILENO);
        close(fd1[1]);

        execvp((*commands).list_cmd[0], (*commands).list_cmd);

        errorMessage("Error: command not found\n");

        exit(1);

    }
    if ((*commands).num_cmd > 2) {
        for (int i = 2; i < (*commands).num_cmd; i++) {
            switch (i) {
                case 2:
                    pipe(fd2);
                    break;
                case 3:
                    pipe(fd3);
                    break;
            }
        }
    }
    if (!(p2 = fork())) {

        if ((*commands).num_cmd == 2) {
            redirect(commands);
        }

        close(fd1[1]);
        dup2(fd1[0], STDIN_FILENO);
        close(fd1[0]);

        if ((*commands).num_cmd > 2) {
            close(fd2[0]);
            if ((*commands).err_redirect2) {
                dup2(fd2[1], STDERR_FILENO);
            }
            dup2(fd2[1], STDOUT_FILENO);
            close(fd2[1]);
        }

        execvp((*commands).list_cmd[(*commands).cmd2], &(*commands).list_cmd[(*commands).cmd2]);

        errorMessage("Error: command not found\n");

        exit(1);
    }
    if ((*commands).num_cmd > 2 && !(p3 = fork())) {

        if ((*commands).num_cmd == 3) {
            redirect(commands);
        }

        close(fd1[0]);
        close(fd1[1]);

        close(fd2[1]);
        dup2(fd2[0], STDIN_FILENO);
        close(fd2[0]);

        if ((*commands).num_cmd > 3) {
            close(fd3[0]);
            if ((*commands).err_redirect3) {
                dup2(fd3[1], STDERR_FILENO);
            }
            dup2(fd3[1], STDOUT_FILENO);
            close(fd3[1]);
        }

        execvp((*commands).list_cmd[(*commands).cmd3], &(*commands).list_cmd[(*commands).cmd3]);

        errorMessage("Error: command not found\n");

        exit(1);
    }
    if ((*commands).num_cmd > 3 && !(p4 = fork())) {

        if ((*commands).num_cmd == 4) {
            redirect(commands);
        }

        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);

        close(fd3[1]);
        dup2(fd3[0], STDIN_FILENO);
        close(fd3[0]);

        execvp((*commands).list_cmd[(*commands).cmd4], &(*commands).list_cmd[(*commands).cmd4]);

        errorMessage("Error: command not found\n");

        exit(1);
    }

    close(fd1[0]);
    close(fd1[1]);

    if ((*commands).num_cmd > 2) {
        for (int i = 2; i < (*commands).num_cmd; i++) {
            switch (i) {
                case 2:
                    close(fd2[0]);
                    close(fd2[1]);
                    break;
                case 3:
                    close(fd3[0]);
                    close(fd3[1]);
                    break;
            }
        }
    }

    waitpid(p1, &status1, 0);
    waitpid(p2, &status2, 0);

    if ((*commands).num_cmd > 2)
        waitpid(p3, &status3, 0);

    if ((*commands).num_cmd > 3)
        waitpid(p4, &status4, 0);

    completionMessage(cmd,
                      WEXITSTATUS(status1), WEXITSTATUS(status2),
                      WEXITSTATUS(status3), WEXITSTATUS(status4),
                      (*commands).num_cmd);

    return true;
}

int main(void) {
    char cmd[CMDLINE_MAX];
    char **env_variables = createEmptyList(MAX_ENV_VAR, MAX_CHAR_ARG + 1);

    while (1) {
        char *nl;
        Cmds commands;
        commands.list_cmd = createEmptyList(MAX_ARG + 1, MAX_CHAR_ARG + 1);

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';

        /* Make cmd into a list and record commands into struct */
        if (!toList(cmd, &commands, env_variables)) {
            completionMessage(cmd, 1, 1, 1, 1, commands.num_cmd);
            delete_list(&commands.list_cmd, MAX_ARG + 1);
            continue;
        }

        /* Builtin command */

        if (!strcmp(cmd, "exit")) {

            fprintf(stderr, "Bye...\n");
            completionMessage(cmd, 0, 0, 0, 0, commands.num_cmd);
            delete_list(&commands.list_cmd, MAX_ARG + 1);
            break;

            /* PWD */
        } else if (!strcmp(commands.list_cmd[0], "pwd")) {
            char curr_dir[MAX_PATH];

            getcwd(curr_dir, sizeof(curr_dir));
            fprintf(stdout, "%s\n", curr_dir);

            completionMessage(cmd, 0, 0, 0, 0, commands.num_cmd);

            /* CD */
        } else if (!strcmp(commands.list_cmd[0], "cd")) {
            if (chdir(commands.list_cmd[1]) == -1) {
                errorMessage("Error: cannot cd into directory\n");
                delete_list(&commands.list_cmd, MAX_ARG + 1);
                completionMessage(cmd, 1, 0, 0, 0, commands.num_cmd);

            } else {
                completionMessage(cmd, 0, 0, 0, 0, commands.num_cmd);
            }

            /* Set */
        } else if (!strcmp(commands.list_cmd[0], "set")) {

            if (!set(&commands, env_variables)) {
                completionMessage(cmd, 1, 1, 1, 1, commands.num_cmd);
                delete_list(&commands.list_cmd, MAX_ARG + 1);
                continue;
            }// sets the letter to be a string

            completionMessage(cmd, 0, 0, 0, 0, commands.num_cmd);

            /* Non-built in commands */
        } else {
            if (commands.num_cmd == 1) {
                /* create a new process for regular commands */
                reg_commands(&commands, cmd);
            } else {
                /* create a up to 4 process for multiple commands */
                pipeline(&commands, cmd);
            }
        }

        delete_list(&commands.list_cmd, MAX_ARG + 1);
    }

    delete_list(&env_variables, MAX_ENV_VAR);

    return EXIT_SUCCESS;
}



