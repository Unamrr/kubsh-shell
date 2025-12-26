#include <stdio.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#define HISTORY_FILE ".kubsh_history"
#define MAX_PATH_LENGTH 1024
#define MAX_ARGS 64

sig_atomic_t signal_received = 0;

//  9:   SIGHUP
volatile sig_atomic_t sighup_received = 0;

void sig_handler(int signum) {
if (signum == SIGHUP) {
sighup_received = 1;
}
}

//  7:   
void print_env_var(const char *var_name) {
    if (var_name == NULL || strlen(var_name) == 0) {
        printf("Usage: \\e $VARNAME\n");
        return;
    }

    //   $   
    if (var_name[0] == '$') {
        var_name++;
    }

    const char *value = getenv(var_name);
    if (value == NULL) {
        printf("Variable '%s' not found.\n", var_name);
        return;
    }


    //  7:    :,  
    if (strchr(value, ':') != NULL) {
        printf("\n");
        char *copy = strdup(value);
        if (!copy) {
            perror("strdup");
            return;
        }

        char *token = strtok(copy, ":");
        while (token != NULL) {
            printf("%s\n", token);
            token = strtok(NULL, ":");
        }
        free(copy);
    } else {
        printf("%s\n", value);
    }
}

//   :   
int is_executable(const char *path) {
    return access(path, X_OK) == 0;
}

//   :    PATH
char *find_in_path(const char *command) {
    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        return NULL;
    }

    //    strtok
    char *path_copy = strdup(path_env);
    if (path_copy == NULL) {
        perror("strdup");
        return NULL;
    }

    char *full_path = malloc(MAX_PATH_LENGTH);
    if (full_path == NULL) {
        perror("malloc");
        free(path_copy);
        return NULL;
    }

    //  PATH  
    char *dir = strtok(path_copy, ":");
    while (dir != NULL) {
        //   :  + / + 
        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", dir, command);
       
        // ,   
        if (is_executable(full_path)) {
            free(path_copy);
            return full_path;
        }
       
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    free(full_path);
    return NULL;
}

//   :  
void fork_exec(char *full_path, int argc, char **argv) {
    	(void)argc;
	int pid = fork();
    if (pid == 0) {
        // child process
        execv(full_path, argv);
        perror("execv");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // parent process
        int status;
        waitpid(pid, &status, 0);
    } else {
        perror("fork");
    }
}

//  5:  echo
void handle_echo(char *args) {
    if (args == NULL || strlen(args) == 0) {
        printf("\n");
        return;
    }
   
    //    "echo"
    char *text = args + 4;
    while (*text == ' ') text++;
   
    //     
    if ((text[0] == '"' && text[strlen(text)-1] == '"') ||
        (text[0] == '\'' && text[strlen(text)-1] == '\'')) {
        text[strlen(text)-1] = '\0';
        text++;
    }
   
    printf("%s\n", text);
}

//     
int parse_arguments(char *input, char **argv) {
    int argc = 0;
    char *token = strtok(input, " \t\n");
   
    while (token != NULL && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
   
    return argc;
}

//  8:  
void execute_command(char *input) {
    char *argv[MAX_ARGS];
    char input_copy[MAX_PATH_LENGTH];
   
    //      
    strncpy(input_copy, input, MAX_PATH_LENGTH - 1);
    input_copy[MAX_PATH_LENGTH - 1] = '\0';
   
    int argc = parse_arguments(input_copy, argv);
    if (argc == 0) {
        return;
    }
   
    //  6:       PATH
    char *full_path = find_in_path(argv[0]);
    if (full_path != NULL) {
        fork_exec(full_path, argc, argv);
        free(full_path);
    } else {
        printf("%s: command not found\n", argv[0]);
    }
}

int main() {
// ============      main() ============
fprintf(stderr, "=== Starting kubsh with VFS support ===\n");

//    VFS   
char vfs_path[512];
const char *home = getenv("HOME");
if (!home) home = "/home/user";
snprintf(vfs_path, sizeof(vfs_path), "%s/users", home);

mkdir(vfs_path, 0755);
fprintf(stderr, "VFS directory: %s\n", vfs_path);

//  VFS   
pid_t vfs_pid = fork();
if (vfs_pid == 0) {
    //   -  VFS
    char *vfs_args[] = {
        "./vfs",        //  vfs
        vfs_path,       //   ~/users
        "-f",           // foreground
        "-s",           // single-threaded
        NULL
    };
    
    fprintf(stderr, "Launching VFS: %s %s -f -s\n", vfs_args[0], vfs_args[1]);
    execv(vfs_args[0], vfs_args);
    perror("Failed to start VFS");
    exit(1);
} else if (vfs_pid > 0) {
    fprintf(stderr, "VFS started with PID: %d\n", vfs_pid);
    fprintf(stderr, "VFS mounted at: %s\n", vfs_path);
    fprintf(stderr, "To create user: mkdir %s/username\n", vfs_path);
    fprintf(stderr, "To delete user: rmdir %s/username\n", vfs_path);
    
    //    
    sleep(1);
} else {
    perror("Failed to fork for VFS");
}
//// ============    VFS ============
    //  9:  SIGHUP
    signal(SIGHUP, sig_handler);
   
    //  4:    
    using_history();
    read_history(HISTORY_FILE);

    fprintf(stderr, "Kubsh started. Type 'exit' or '\\q' to quit.\n");
   
    char *input;
    while (true) {
        input = readline("$ ");
       
if (sighup_received){
printf("Configuration reloaded\n");
sighup_received = 0;
}
       
        //  2:   Ctrl+D
        if (input == NULL) {
            printf("\n");
            break; // Ctrl+D
        }
       
        //   
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
       
        //  4:     
        add_history(input);
        write_history(HISTORY_FILE);
       
        //  3:   \q
        if (strcmp(input, "exit") == 0 || strcmp(input, "\\q") == 0) {
            printf("Exiting kubsh.\n");
            free(input);
            break;
        }
        //  5:  echo
        else if (strncmp(input, "echo", 4) == 0) {
            handle_echo(input);
        }
        //  7:   
        else if (strncmp(input, "\\e", 2) == 0) {
            char *var_name = input + 2;
            while (*var_name == ' ') var_name++;
            print_env_var(var_name);
        }
//10 punkt
else if (strncmp(input, "\\l", 2) == 0) {
    char *device = input + 2;
    while (*device == ' ') device++;
    
    if (strlen(device) == 0) {
        printf("Usage: \\l /dev/sda\n");
    } else {
        char command[256];
        snprintf(command, sizeof(command), 
                 "sudo fdisk -l %s 2>/dev/null || lsblk %s 2>/dev/null", 
                 device, device);
        int result = system(command);
        if (result != 0) {
            printf("Cannot get disk info for %s\n", device);
        }
    }
}
else if (strcmp(input, "history") == 0) {
	HIST_ENTRY **hist = history_list();
	if(hist){
		for(int i = 0; hist[i]; i++){
			printf("%5d %s\n", i + 1, hist[i]->line);
		}
	}
}

        //  8:      PATH
        //  6:   
        else {
            execute_command(input);
        }
       
        free(input);
    }
   
    //  4:    
    write_history(HISTORY_FILE);
    return 0;
}
}

