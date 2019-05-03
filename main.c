#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <wait.h>

#define MAX_ARGS 512

typedef struct Job {
    pid_t pid;
    char command[MAX_ARGS];
    struct Job *next;
} Job;

void killBackJobs(Job *head) {
    Job *prev, *next;
    prev = head->next;
    while (prev->next != NULL) {
        next = prev->next;
        kill(prev->pid, SIGKILL);
        free(prev);
        prev = next;
    }
    kill(prev->pid, SIGKILL);
    free(prev);
}

bool waitCheck(char **args) {
    int j = 0;
    while (args[j] != NULL) {
        j++;
    }
    --j; //get last argument
    if (strchr(args[j], '&') == NULL) {
        return true;
    }
    return false;
}

char *removeChar(char token[MAX_ARGS], char c) {
    int i = 0;
    char *newToken = token;
    while (newToken[i] != c) {
        i++;
    }
    newToken[i] = '\0';
    return newToken;
}

void handleJobs(Job *last) {
    Job *check, *temp;
    int status;
    check = last->next;
    while (check != NULL) {
        status = waitpid(check->pid, NULL, WNOHANG);
        if (status == check->pid) { //CHECK IF PROCESS FINISHED ALREADY
            last->next = check->next;
            temp = check;
            check = check->next;
            free(temp);
        } else {
            printf("%d %s\n", check->pid, check->command);
            last = check;
            check = check->next;
        }
    }
}

void handleCD(char *const args[MAX_ARGS], char *lastLocation) {
    char currLocation[MAX_ARGS];
    char newPath[MAX_ARGS];
    char c = '\n';
    removeChar(args[1], c);
    //handle cases like: "cd " , "cd ~"
    if ((!strcmp(args[1], "")) || (!strcmp(args[1], "~"))) {
        getcwd(lastLocation, sizeof(lastLocation)); //save location before cd
        chdir(getenv("HOME"));
    }
        // handle cases like "cd -"
    else if (!strcmp(args[1], "-")) {
        if (!strcmp(lastLocation, "")) { //check in exists last location
            printf("cd: OLDPWD not set\n");
            return;
        }
        getcwd(currLocation, sizeof(currLocation)); //save current location
        chdir(lastLocation);
        printf("%s\n", lastLocation);
        lastLocation = currLocation;
    } else {
        getcwd(lastLocation, MAX_ARGS); //save location before cd
        if ((!(strncmp(args[1], "\"", 1))) || (!strncmp(args[1], "'", 1))) {
            c = args[1][0];
            strncpy(newPath, args[1] + 1, strlen(args[1]) - 1); //remove first '
            removeChar(newPath, c); //remove last '
            chdir(newPath);
        } else {
            chdir(args[1]);
        }
    }
}

int main() {
    pid_t pid;
    char *lastToken;
    char oldPwd[MAX_ARGS];
    Job *prevJ, *newJ;
    bool isToWait;

    char buffer[MAX_ARGS];
    Job *head = (Job *) malloc(sizeof(Job));
    head->pid = getpid();
    prevJ = head;

    while (true) {
        char backUpBuffer[MAX_ARGS];
        int i = 0;
        char *tokens[MAX_ARGS] = {0};
        printf("> ");
        fgets(buffer, sizeof(buffer), stdin); //get command input as char*
        strcpy(backUpBuffer, buffer);
        //get first token of buffer input.
        tokens[0] = strtok(buffer, " ");
        //handle input case of: "cd" ONLY
        if (!strcmp(tokens[0], "cd\n")) {
            printf("%d\n", getpid());
            getcwd(oldPwd, sizeof(oldPwd));
            chdir(getenv("HOME"));
            continue;
        }
        //handle input case of "cd" + else
        if (!strcmp(tokens[0], "cd")) {
            printf("%d\n", getpid());
            tokens[++i] = strtok(NULL, ""); //all the rest is in [1] index
            handleCD(tokens, oldPwd);
            continue;
        }
        //not cd command, so split the buffer by spaces
        while (tokens[i] != NULL) {
            tokens[++i] = strtok(NULL, " ");
        }
        isToWait = waitCheck(tokens); //check if parent need to wait for child
        if (isToWait) { //if no & - remove the '\n'
            lastToken = removeChar(tokens[--i], '\n');
            tokens[i] = lastToken;
        } else { // if need to wait - remove '&'
            tokens[--i] = NULL;
        }
        //if we receive the built in command "exit"
        if (!strcmp(tokens[0], "exit")) {
            printf("%d", getpid());
            if (head->next != NULL) {
                killBackJobs(head); //kill background jobs and free ALL structs
            }
            free(head);
            break;
        }
        //"jobs" command will print all the background jobs
        if (!strcmp(tokens[0],"jobs")) {
            if (head->next != NULL) {
                handleJobs(head);
            }
            continue;
        }
        if ((pid = fork()) < 0) { //fork new process
            printf("fork error");
        } else if (pid == 0) { // if in child
            int status = execvp(tokens[0], tokens);
            if (status == -1) {
                fprintf(stderr,"Error in system call");
            }
            exit(0); //exit child
        } else { //in parent
            printf("%d\n", pid);
            if (isToWait) { //if wait we dont add the child to the linked list.
                waitpid(pid, NULL, 0);
            } else { //if here - means we run in background (got &)
                newJ = (Job *) malloc(sizeof(Job)); //create new Job structure
                newJ->pid = pid;    //set child's pid
                strcpy(newJ->command,removeChar(backUpBuffer,'&'));
                prevJ->next = newJ; //parent points to child in linked list.
                prevJ = newJ;
            }
        }
    }
    return 0;
}