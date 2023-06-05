#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int SuperMadisonShell();
int lexer(char *line, char ***args, int *num_args);
int lexer_cmds(char *line, char ***lines, int *num_cmd);
int lexer_pipes(char *line);
char *strtrim(char const *str);
int validstr(char const *str);
int is_white_space(char c);
int ExecuteArgs(char** args, int num_args);
int hasRedirection(char** string, int num_args);

int main() {
    while (1){
        SuperMadisonShell();
    }
    return 0;
}

int SuperMadisonShell(){
    char *buf = malloc(sizeof(char) * 1000);
    printf("smash> ");
    fflush(stdout);
    size_t bufsize = 1000;
    getline(&buf, &bufsize, stdin);
    if(strcmp(buf, "exit\n") == 0){
        exit(1);
    }
    if (validstr(buf)){
        char ***lines_hold = malloc(sizeof(char***));
        int num_lines;
        if (!lexer_cmds(buf, lines_hold, &num_lines)){
            //if (lexer_pipes(buf) > 0) return 0;
            char ***args_hold = malloc(sizeof(char***));
            int num_args = 0;
            if (lexer(buf, args_hold, &num_args) == 0){
                char** args = (*args_hold); //load the 2D argument character array
                ExecuteArgs(args, num_args);
            } else{
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
                return -1;
            }
        } else{
            for (int i = 0; i < num_lines; ++i) {
                buf = (*lines_hold)[i];
                //if (lexer_pipes(buf) > 0) break;
                char ***args_hold = malloc(sizeof(char***));
                int num_args = 0;
                if (lexer(buf, args_hold, &num_args) == 0){
                    char** args = (*args_hold); //load the 2D argument character array
                    ExecuteArgs(args, num_args);
                } else{
                    char error_message[30] = "An error has occurred\n";
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    return -1;
                }
            }
        }
    }
    free(buf);
    return 0;
}

int lexer(char *line, char ***args, int *num_args){
    *num_args = 0;
    // count number of args
    char *l = strdup(line);
    if(l == NULL){
        return -1;
    }
    char *token = strtok(l, " \t\n");
    while(token != NULL){
        (*num_args)++;
        token = strtok(NULL, " \t\n");
    }
    free(l);
    // split line into args
    *args = malloc(sizeof(char **) * (*num_args + 1));
    *num_args = 0;
    token = strtok(line, " \t\n");
    while(token != NULL){
        char *token_copy = strdup(token);
        if(token_copy == NULL){
            return -1;
        }
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " \t\n");
    }
    return 0;
}

int validstr(char const *str) {
    for (int i = 0; str[i] != '\0'; ++i) {
        if (!is_white_space(str[i]))
            return 1;
    }
    return 0;
}

int ExecuteArgs(char** args, int num_args){
    //Check for built in commands
    if (strcmp(args[0], "exit") == 0){
        if (num_args > 1){
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
        }
        else{
            exit(1);
        }
    }
    else if (strcmp(args[0], "cd") == 0){
        if (num_args == 0 || num_args > 2){
            //char error_message[30] = "An error has occurred\n";
            //write(STDERR_FILENO, error_message, strlen(error_message));
            return -1;
        }
        if(chdir(args[1]) != 0){
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            return -1;
        }
        return 0;
    }
    else if (strcmp(args[0], "pwd") == 0){
        char buf[256];
        getcwd(buf, sizeof(buf));
        printf("%s\n", buf);
        return 0;
    }
    else if (strcmp(args[0], "loop") == 0){
        int repetition = atoi(args[1]);
        for (int i = 0; i < repetition; ++i) {
            ExecuteArgs(&args[2], (num_args-2));
        }
        return 0;
    }
    //Not a built in command
    int redirection = hasRedirection(args, num_args);
    //execute below if there are no redirections
    if (redirection == -1){
        pid_t child_pid = fork();
        if (child_pid < 0){
            return -1;
        } else if(child_pid == 0){ //in the child process
            if (strcmp(args[0], "/bin/ls")==0 || strcmp(args[0], "ls")==0){
                int devnull_fd = open("/dev/null", O_WRONLY); // open /dev/null for writing
                int saved_stderr_fd = dup(STDERR_FILENO); // save the original stderr file descriptor
                dup2(devnull_fd, STDERR_FILENO); // redirect stderr to /dev/null
                close(devnull_fd); // close the file descriptor, we no longer need it
                execv(args[0], args);
                dup2(saved_stderr_fd, STDERR_FILENO); // restore the original stderr file descriptor
                close(saved_stderr_fd); // close the saved file descriptor
            }
            else{
                printf("running else\n");
                execv(args[0], args);
            }

        } else{
            int status;
            waitpid(child_pid, &status, 0);
        }
    }
    // execute below if there are redirections
    else{
        pid_t child_pid = fork();
        if (child_pid < 0){
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            return -1;
        } else if(child_pid == 0){ //in the child process
            int fd = open(args[redirection+1], O_WRONLY | O_CREAT);
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
            args[redirection] = NULL;
            if (execv(args[0], args) == -1){
                //char error_message[30] = "An error has occurred\n";
                //write(STDERR_FILENO, error_message, strlen(error_message));
                return -1;
            }
        } else{
            int status;
            waitpid(child_pid, &status, 0);
        }
    }
    return 0;
}

/*
 * The functions below are required for the string trimming method and all work in conjunction
 * */
int is_white_space(char c) {
    return (c == ' ' || c == '\t' || c == '\n');
}
int get_first_position(char const *str) {
    int i = 0;
    while (is_white_space(str[i])) {
        i += 1;
    }
    return (i);
}
int get_str_len(char const *str) {
    int len = 0;
    while (str[len] != '\0') {
        len += 1;
    }
    return (len);
}
int get_last_position(char const *str) {
    int i = get_str_len(str) - 1;
    while (is_white_space(str[i])) {
        i -= 1;
    }
    return (i);
}
int get_trim_len(char const *str) {
    return (get_last_position(str) - get_first_position(str));
}
/*
 * END of String trimming funtions
 * */
char *strtrim(char const *str) {
    char *trim = NULL;
    int i, len, start, end;
    if (str != NULL) {
        i = 0;
        len = get_trim_len(str) + 1;
        trim = (char *)malloc(len);
        start = get_first_position(str);
        while (i < len) {
            trim[i] = str[start];
            i += 1;
            start += 1;
        }
        trim[i] = '\0';
    }
    return (trim);
}

int hasRedirection(char** string, int num_args){
    for (int i = 0; i < num_args; ++i) {
        if (string[i][0] == '>'){
            return i;
        }
    }
    return -1;
}

int lexer_cmds(char *line, char ***lines, int *num_cmd){
    int found_colon = 0;
    for (int i = 0; line[i] != '\0'; ++i) {
        if (line[i] == ';'){
            found_colon++;
            break;
        }
    }

    *num_cmd = 0;

    char *l = strdup(line);
    if(l == NULL){
        return -1;
    }
    char *token = strtok(l, ";\n");
    while(token != NULL){
        (*num_cmd)++;
        token = strtok(NULL, ";\n");
    }

    *lines = malloc(sizeof(char **) * (*num_cmd));
    *num_cmd = 0;
    token = strtok(line, ";\n");
    while(token != NULL){
        char *token_copy = strdup(token);
        if(token_copy == NULL){
            return 0;
        }
        (*lines)[(*num_cmd)++] = token_copy;
        token = strtok(NULL, ";\n");
    }
    return found_colon;
}
