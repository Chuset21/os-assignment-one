#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdint.h>

#define ARGS_SIZE 30

/**
 * Tokenize a string so that it can easily be read for commands
 * @param buffer string to be tokenized
 * @param bufferEnd last index of the buffer string
 * @param args array of string tokens to be populated
 * @param background pointer to bool to be populated, true if the command should be run in the background
 * @return the amount of tokens populated into args
 */
int getcmd(char *buffer, ssize_t bufferEnd, char *args[], bool *background) {
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
            args[i++] = token;
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
static char *getLine(ssize_t *bufferLength) {
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
 * Use the given command
 * @param args command
 * @param commandLength the amount of tokens in args
 * @param background whether the command should run in the background
 */
void useCommand(char *args[], int commandLength, bool background) {
    if (commandLength > 0) {
        if (commandLength > ARGS_SIZE) {
            printf("Arguments exceeded max size\n");
        } else {
            const pid_t childPID = fork();
            if (childPID) {
                if (!background) {
                    int status = 0;
                    waitpid(childPID, &status, WUNTRACED);
                }
            } else {
                execvp(*args, args);
                printf("Failed to execute command\n");
                exit(127);
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
    bool background = false;
    ssize_t bufLen = 0;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (1) {
        char *const cwd = getcwd(NULL, 0);
        printf("%s > ", cwd);
        free(cwd);
        char *const buffer = getLine(&bufLen);
        if (buffer != NULL) {
            const int commandLength = getcmd(buffer, bufLen - 1, args, &background);
            useCommand(args, commandLength, background);
            free(buffer);
        }
    }
#pragma clang diagnostic pop
}
