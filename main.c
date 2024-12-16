#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_ENV_VARS 64

typedef struct {
    char *name;
    char *value;
} EnvVar;

EnvVar env_vars[MAX_ENV_VARS];
int env_var_count = 0;

void set_env_var(const char *name, const char *value) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            free(env_vars[i].value);
            env_vars[i].value = strdup(value);
            return;
        }
    }
    env_vars[env_var_count].name = strdup(name);
    env_vars[env_var_count].value = strdup(value);
    env_var_count++;
}

void unset_env_var(const char *name) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            free(env_vars[i].name);
            free(env_vars[i].value);
            env_vars[i] = env_vars[--env_var_count];
            return;
        }
    }
}

char* get_env_var(const char *name) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

void replace_env_vars(char *input) {
    char buffer[MAX_INPUT];
    char *start = input;
    char *end;
    buffer[0] = '\0';

    while ((start = strchr(start, '$')) != NULL) {
        strncat(buffer, input, start - input);
        start++;
        end = start;
        while (*end && (isalnum(*end) || *end == '_')) end++;
        char var_name[MAX_INPUT];
        strncpy(var_name, start, end - start);
        var_name[end - start] = '\0';
        char *value = get_env_var(var_name);
        if (value) strcat(buffer, value);
        input = end;
        start = end;
    }
    strcat(buffer, input);
    strcpy(input, buffer);
}

void execute_command(char **args, int background, int input_fd, int output_fd) {
    pid_t pid = fork();
    if (pid == 0) {
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if (!background) {
            waitpid(pid, NULL, 0);
        }
    } else {
        perror("fork");
    }
}

void parse_and_execute(char *input) {
    char *args[MAX_ARGS];
    int arg_count = 0;
    int background = 0;
    int input_fd = STDIN_FILENO;
    int output_fd = STDOUT_FILENO;

    replace_env_vars(input);

    char *token = strtok(input, " \t\n");
    while (token != NULL) {
        if (strcmp(token, "&") == 0) {
            background = 1;
        } else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) {
                input_fd = open(token, O_RDONLY);
                if (input_fd < 0) {
                    perror("open");
                    return;
                }
            }
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\n");
            if (token) {
                output_fd = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd < 0) {
                    perror("open");
                    return;
                }
            }
        } else {
            args[arg_count++] = token;
        }
        token = strtok(NULL, " \t\n");
    }
    args[arg_count] = NULL;

    if (arg_count == 0) return;

    if (strcmp(args[0], "cd") == 0) {
        if (arg_count > 1) {
            if (chdir(args[1]) != 0) {
                perror("chdir");
            }
        } else {
            fprintf(stderr, "cd: missing argument\n");
        }
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[MAX_INPUT];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("getcwd");
        }
    } else if (strcmp(args[0], "set") == 0) {
        if (arg_count == 3) {
            set_env_var(args[1], args[2]);
        } else {
            fprintf(stderr, "set: invalid arguments\n");
        }
    } else if (strcmp(args[0], "unset") == 0) {
        if (arg_count == 2) {
            unset_env_var(args[1]);
        } else {
            fprintf(stderr, "unset: invalid arguments\n");
        }
    } else {
        execute_command(args, background, input_fd, output_fd);
    }

    if (input_fd != STDIN_FILENO) close(input_fd);
    if (output_fd != STDOUT_FILENO) close(output_fd);
}

int main() {
    char input[MAX_INPUT];

    while (1) {
        printf("xsh# ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        if (strcmp(input, "quit\n") == 0 || strcmp(input, "exit\n") == 0) {
            break;
        }
        parse_and_execute(input);
    }

    return 0;
}
