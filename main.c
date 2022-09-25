#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/fcntl.h>

#define ARGS_SIZE 30


typedef struct job {
    char *name;
    pid_t pid;
} job;

typedef struct node {
    job *data;
    struct node *next;
} node;

node *head = NULL;

/**
 * Creates a job
 * @param name name of the job
 * @param pid pid of the child
 * @return pointer to the created job
 */
job *createJob(char *const name, pid_t pid) {
    job *const result = malloc(sizeof(job));
    result->name = name;
    result->pid = pid;
    return result;
}

/**
 * Creates a node
 * @param name name of the job
 * @param pid pid of the child
 * @return pointer to the created node
 */
node *createNode(char *const name, pid_t pid) {
    node *const result = malloc(sizeof(node));
    result->data = createJob(name, pid);
    result->next = NULL;
    return result;
}

/**
 * Adds job to the linked list
 * @param name name of the job
 * @param pid pid of the child
 */
void addNode(char *const name, pid_t pid) {
    node *const temp = createNode(name, pid);
    if (head == NULL) {
        head = temp;
    } else {
        node *cur = head;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = temp;
    }
}

/**
 * Removes the xth entry from the list
 * @param x index of entry from the list to be removed
 * @return NULL if unsuccessful, otherwise the pointer to the removed node
 */
node *removeNode(int x) {
    node *cur = head;
    node *prev = NULL;
    for (int i = 1; i < x && cur != NULL; ++i) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        return NULL;
    }

    if (cur == head) {
        head = cur->next;
    } else {
        prev->next = cur->next;
    }

    return cur;
}

/**
 * Executes the jobs command
 * @param params parameters for command (should be empty)
 */
void jobs(char *params[]) {
    if (*params != NULL) {
        fprintf(stderr, "jobs: too many arguments\n");
        return;
    }

    int index = 1;
    for (node *cur = head; cur != NULL; cur = cur->next, ++index) {
        printf("[%d]\t%s\n", index, cur->data->name);
    }
}

/**
 * Executes the fg command
 * @param params parameters for command
 */
void fg(char *params[]) {
    int index = 1;
    if (*params != NULL) {
        if (params[1] != NULL) {
            fprintf(stderr, "fg only accepts 1 argument\n");
            return;
        } else {
            index = (int) strtol(*params, NULL, 10);
        }
    }
    if (index < 1) {
        fprintf(stderr, "fg only accepts positive indexes\n");
        return;
    }

    node *const pNode = removeNode(index);
    if (pNode == NULL) {
        fprintf(stderr, "fg given invalid index [%d]\n", index);
        return;
    }

    int status = 0;
    waitpid(pNode->data->pid, &status, WUNTRACED);

    free(pNode->data->name);
    free(pNode->data);
    free(pNode);
}

/**
 * Executes the echo command
 * @param params parameters for command
 */
void echo(char *params[]) {
    while (params[1] != NULL) {
        printf("%s ", *params++);
    }
    puts(*params);
}

/**
 * Executes the print working directory command
 * @param params parameters for command (should be empty)
 */
void pwd(char *params[]) {
    if (*params == NULL) {
        char *const cwd = getcwd(NULL, 0);
        puts(cwd);
        free(cwd);
    } else {
        fprintf(stderr, "pwd: too many arguments\n");
    }
}

/**
 * Executes the change directory command
 * @param params parameters for command
 */
void cd(char *params[]) {
    if (*params != NULL) {
        if (params[1] != NULL) {
            fprintf(stderr, "cd only accepts 1 argument\n");
        } else {
            if (chdir(*params)) {
                perror(NULL);
            }
        }
    } else {
        pwd(params);
    }
}

/**
 * Kill all processes
 */
void exitShell() {
    kill(0, SIGTERM);
}

/**
 * Executes a cmd if it matches the description of a built in cmd
 * @param cmd command to match against and execute
 * @param params parameters for given command
 * @return true if the command was matched and run, false otherwise
 */
bool isBuiltIn(char *cmd, char *params[]) {
    if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "fg") == 0) {
    } else if (strcmp(cmd, "pwd") == 0) {
        pwd(params);
    } else if (strcmp(cmd, "jobs") == 0) {
        jobs(params);
    } else if (strcmp(cmd, "exit") == 0) {
        exitShell();
    } else if (strcmp(cmd, "echo") == 0) {
        echo(params);
    } else {
        return false;
    }
    return true;
}

/**
 * Tokenize a string so that it can easily be read for commands
 * @param buffer string to be tokenized
 * @param bufferEnd last index of the buffer string
 * @param args array of string tokens to be populated
 * @param background pointer to bool to be populated, true if the command should be run in the background
 * @param outputRedirection pointer to string to be populated, NULL if no output redirection will take place
 * @param cmdPipeIndex pointer to int to be populated, will equal -1 if no command piping is to take place
 * @return the amount of tokens populated into args
 */
int
getcmd(char *buffer, ssize_t bufferEnd, char *args[], bool *background, char **outputRedirection, int *cmdPipeIndex) {
    *cmdPipeIndex = -1;
    *outputRedirection = NULL;
    *background = false;
    int i = 0;
    char *token;

    bool exit = false;
    // Check if background is specified
    while (!exit && bufferEnd >= 0 && (buffer[bufferEnd] == '&' || isspace(buffer[bufferEnd]))) {
        if (buffer[bufferEnd] == '&') {
            *background = true;
            buffer[bufferEnd] = ' ';
            exit = true;
        }
        bufferEnd--;
    }
    // Skip tokenizing if string is empty
    if (bufferEnd < 0) {
        return 0;
    }

    while ((token = strsep(&buffer, " \t")) != NULL) {
        if (strlen(token) > 0) {
            if (i > ARGS_SIZE) {
                return INT32_MAX;
            }
            if (strcmp(token, ">") == 0) {
                char *temp = buffer;
                if ((token = strsep(&temp, " \t")) != NULL) {
                    *outputRedirection = token;
                    strsep(&buffer, " \t"); // To skip this instance
                } else {
                    fprintf(stderr, "Error parsing '>'");
                }
            } else if (strcmp(token, "|") == 0) {
                if (i == 0 ||
                    *cmdPipeIndex > 0) { // If it's the first token or there's already been a pipe, then error out
                    fprintf(stderr, "Error parsing '|'");
                } else {
                    args[i++] = NULL;
                    *cmdPipeIndex = i;
                }
            } else {
                args[i++] = token;
            }
        }
    }
    args[i] = NULL;

    return i;
}

/**
 * Safely read a line from stdin
 * @param bufferLength pointer to be updated as the length variable of the read string
 * @return string containing the line fed into stdin
 */
static char *getLine(ssize_t *const bufferLength) {
    char *buffer = NULL;
    size_t size = 0;
    *bufferLength = getline(&buffer, &size, stdin);
    if (*bufferLength < 2) {
        free(buffer);
        // Exit if CTRL+D was pressed
        if (*bufferLength < 1) {
            exit(0);
        }
        return NULL;
    }

    buffer[--(*bufferLength)] = '\0';
    return buffer;
}

/**
 * Run the given command
 * @param args command
 * @param outputRedirection where the output should be redirected, if NULL no redirection will take place
 */
void runCmd(char *args[], const char *const outputRedirection) {
    int output = -1;
    int prevOut;
    if (outputRedirection != NULL) {
        fflush(stdout);
        output = open(outputRedirection, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (output < 0) {
            perror("error opening file");
            exit(127);
        }
        prevOut = dup(fileno(stdout));
        if (dup2(output, fileno(stdout)) < 0) {
            perror("error redirecting stdout");
            exit(127);
        }
    }

    if (isBuiltIn(*args, args + 1)) {
        if (output >= 0) {
            fflush(stdout);
            close(output);
            dup2(prevOut, fileno(stdout));
            close(prevOut);
        }
        exit(0);
    } else {
        execvp(*args, args);
        printf("Failed to execute command\n");
        exit(127);
    }
}

/**
 * Use the given command/s
 * @param command command as a string
 * @param args command/s (tokenized)
 * @param commandLength the amount of tokens in args
 * @param background whether the command should run in the background
 * @param outputRedirection where the output should be redirected, if NULL no redirection will take place
 * @param cmdPipeIndex the next index after where the pipe was found, if -1 then no command piping will take place
 */
void
useCommand(char *const command, char *args[], int commandLength, bool background, const char *const outputRedirection,
           int cmdPipeIndex) {
    if (commandLength > 0) {
        if (commandLength > ARGS_SIZE) {
            printf("Arguments exceeded max size\n");
        } else {
            const pid_t childPID = fork();
            if (childPID) {
                if (strcmp(*args, "cd") == 0) {
                    cd(args + 1);
                } else if (strcmp(*args, "fg") == 0) {
                    fg(args + 1);
                } else if (!background) {
                    int status = 0;
                    waitpid(childPID, &status, WUNTRACED);
                    free(command);
                } else {
                    addNode(command, childPID);
                }
            } else {
                if (cmdPipeIndex > 0) {
                    int fileDescriptors[2];
                    pipe(fileDescriptors);
                    if (fork() == 0) { // This is the child
                        dup2(fileDescriptors[1], fileno(stdout));
                        runCmd(args, outputRedirection);
                    }

                    // This is the parent
                    dup2(fileDescriptors[0], fileno(stdin));
                    close(fileDescriptors[1]); // Close write end of pipe
                    runCmd(args + cmdPipeIndex, outputRedirection); // Start the commands to the right of the pipe
                } else {
                    runCmd(args, outputRedirection);
                }
            }
        }
    } else if (background) {
        printf("Command cannot run in background with no arguments\n");
    }
}

// This will be the parent pid, it'll never change or be mutated
pid_t parent;

/**
 * Handler for SIGINT signal
 * @param sig Signal code
 */
static void signalHandler(int sig) {
    if (sig == SIGINT) {
        // If the process isn't the parent, call exit
        pid_t cur = getpid();
        if (parent != cur) {
            exit(0);
        }
    }
}

int main(void) {
    signal(SIGINT, signalHandler);
    // This will ignore the CTRL+Z signal
    signal(SIGTSTP, SIG_IGN);
    parent = getpid();

    char *args[ARGS_SIZE + 1];
    char *outputRedirection = NULL;
    bool background = false;
    int cmdPipeIdx = -1;
    ssize_t bufLen = 0;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (1) {
        char *const cwd = getcwd(NULL, 0);
        printf("%s > ", cwd);
        fflush(stdout);
        free(cwd);
        char *const buffer = getLine(&bufLen);
        if (buffer != NULL) {
            const int commandLength = getcmd(buffer, bufLen - 1, args, &background, &outputRedirection, &cmdPipeIdx);
            useCommand(buffer, args, commandLength, background, outputRedirection, cmdPipeIdx);
        }
    }
#pragma clang diagnostic pop
}
