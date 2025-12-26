#define FUSE_USE_VERSION 31
#define MAX_USERS 1000
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

static int vfs_pid = -1;

// === ВСТАВКА: Структура для хранения информации о пользователях ===
typedef struct {
    char name[256];
    uid_t uid;
    char home[1024];
    char shell[1024];
} user_info_t;

static user_info_t *users_list = NULL;
static int users_count = 0;
// === КОНЕЦ ВСТАВКИ ===
int get_users_list() {
    printf("VFS: Loading users...\n");
    
    // Всегда создаём хотя бы этих пользователей
    users_count = 2;
    users_list = malloc(users_count * sizeof(user_info_t));
    
    if (!users_list) {
        perror("malloc");
        return -1;
    }
    
    // Пользователь 1: текущий (обязательно)
    strncpy(users_list[0].name, "user", sizeof(users_list[0].name) - 1);
    users_list[0].uid = 1000;
    strncpy(users_list[0].home, "/home/user", sizeof(users_list[0].home) - 1);
    strncpy(users_list[0].shell, "/bin/bash", sizeof(users_list[0].shell) - 1);
    
    // Пользователь 2: root (обязательно)
    strncpy(users_list[1].name, "root", sizeof(users_list[1].name) - 1);
    users_list[1].uid = 0;
    strncpy(users_list[1].home, "/root", sizeof(users_list[1].home) - 1);
    strncpy(users_list[1].shell, "/bin/bash", sizeof(users_list[1].shell) - 1);
    
    printf("VFS: Created %d users (user, root)\n", users_count);
    return users_count;
}


void free_users_list() {
    // === ВСТАВКА: Освобождение памяти ===
    if (users_list) {
        free(users_list);
        users_list = NULL;
    }
    users_count = 0;
    // === КОНЕЦ ВСТАВКИ ===
}

static int users_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi,
    enum fuse_readdir_flags flags
) {
    // === ВСТАВКА: Реализация чтения директории ===
    printf("readdir: %s\n", path);
   
    if (strcmp(path, "/") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
       
        for (int i = 0; i < users_count; i++) {
            filler(buf, users_list[i].name, NULL, 0, 0);
        }
        return 0;
    }
   
    char username[256];
    if (sscanf(path, "/%255[^/]", username) == 1) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        filler(buf, "id", NULL, 0, 0);
        filler(buf, "home", NULL, 0, 0);
        filler(buf, "shell", NULL, 0, 0);
        return 0;
    }
   
    return -ENOENT;
    // === КОНЕЦ ВСТАВКИ ===
}

static int users_open(const char *path, struct fuse_file_info *fi) {
    printf("open: %s\n", path);
    return 0;
}

static int users_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    // === ВСТАВКА: Реализация чтения файлов ===
    printf("read: %s, size: %zu, offset: %ld\n", path, size, offset);
   
    char username[256], filename[256];
    if (sscanf(path, "/%255[^/]/%255s", username, filename) == 2) {
        for (int i = 0; i < users_count; i++) {
            if (strcmp(users_list[i].name, username) == 0) {
                char content[1024];
                size_t content_len = 0;
               
                if (strcmp(filename, "id") == 0) {
                    content_len = snprintf(content, sizeof(content), "%d\n", users_list[i].uid);
                } else if (strcmp(filename, "home") == 0) {
                    content_len = snprintf(content, sizeof(content), "%s\n", users_list[i].home);
                } else if (strcmp(filename, "shell") == 0) {
                    content_len = snprintf(content, sizeof(content), "%s\n", users_list[i].shell);
                } else {
                    return -ENOENT;
                }
               
                if (offset >= content_len) {
                    return 0;
                }
               
                if (offset + size > content_len) {
                    size = content_len - offset;
                }
               
                memcpy(buf, content + offset, size);
                return size;
            }
        }
    }
   
    return -ENOENT;
    // === КОНЕЦ ВСТАВКИ ===
}

static int users_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
    // === ВСТАВКА: Реализация получения атрибутов ===
    printf("getattr: %s\n", path);
    memset(stbuf, 0, sizeof(struct stat));
   
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
   
    char username[256];
    if (sscanf(path, "/%255[^/]", username) == 1 && strchr(path + 1, '/') == NULL) {
        for (int i = 0; i < users_count; i++) {
            if (strcmp(users_list[i].name, username) == 0) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                return 0;
            }
        }
    }
   
    char user_dir[256], filename[256];
    if (sscanf(path, "/%255[^/]/%255s", user_dir, filename) == 2) {
        for (int i = 0; i < users_count; i++) {
            if (strcmp(users_list[i].name, user_dir) == 0) {
                if (strcmp(filename, "id") == 0 ||
                    strcmp(filename, "home") == 0 ||
                    strcmp(filename, "shell") == 0) {
                    stbuf->st_mode = S_IFREG | 0644;
                    stbuf->st_nlink = 1;
                   
                    if (strcmp(filename, "id") == 0) {
                        stbuf->st_size = snprintf(NULL, 0, "%d\n", users_list[i].uid);
                    } else if (strcmp(filename, "home") == 0) {
                        stbuf->st_size = snprintf(NULL, 0, "%s\n", users_list[i].home);
                    } else if (strcmp(filename, "shell") == 0) {
                        stbuf->st_size = snprintf(NULL, 0, "%s\n", users_list[i].shell);
                    }
                    return 0;
                }
            }
        }
    }
   
    return -ENOENT;
    // === КОНЕЦ ВСТАВКИ ===
}

// === ВСТАВКА: Функция для выполнения системных команд ===
int execute_system_command(const char *command) {
    printf("Executing: %s\n", command);
    int result = system(command);
    if (result == -1) {
        perror("system");
        return -1;
    }
    return WEXITSTATUS(result);
}
// === КОНЕЦ ВСТАВКИ ===

// === ВСТАВКА: Функция для создания реального пользователя ===
int create_system_user(const char *username) {
    char command[512];
    snprintf(command, sizeof(command), "sudo adduser --disabled-password --gecos '' %s", username);
   
    if (execute_system_command(command) != 0) {
        fprintf(stderr, "Failed to create user: %s\n", username);
        return -1;
    }
   
    printf("User %s created successfully\n", username);
    return 0;
}
// === КОНЕЦ ВСТАВКИ ===

// === ВСТАВКА: Функция для удаления реального пользователя ===
int delete_system_user(const char *username) {
    char command[256];
    snprintf(command, sizeof(command), "sudo userdel -r %s", username);
   
    if (execute_system_command(command) != 0) {
        fprintf(stderr, "Failed to delete user: %s\n", username);
        return -1;
    }
   
    printf("User %s deleted successfully\n", username);
    return 0;
}
// === КОНЕЦ ВСТАВКИ ===

static int users_mkdir(const char *path, mode_t mode) {
    // === ВСТАВКА: Реализация создания каталога (пункт 11) ===
    printf("mkdir: %s\n", path);
   
    char username[256];
    if (sscanf(path, "/%255[^/]", username) == 1 && strchr(path + 1, '/') == NULL) {
        printf("Creating user directory: %s\n", username);
       
        if (create_system_user(username) == 0) {
            free_users_list();
            get_users_list();
            return 0;
        } else {
            return -EACCES;
        }
    }
   
    return -EEXIST;
    // === КОНЕЦ ВСТАВКИ ===
}

// === ВСТАВКА: Реализация удаления каталога (пункт 11) ===
static int users_rmdir(const char *path) {
    printf("rmdir: %s\n", path);
   
    char username[256];
    if (sscanf(path, "/%255[^/]", username) == 1 && strchr(path + 1, '/') == NULL) {
        printf("Deleting user directory: %s\n", username);
       
        if (delete_system_user(username) == 0) {
            free_users_list();
            get_users_list();
            return 0;
        } else {
            return -EACCES;
        }
    }
   
    return -ENOENT;
}
// === КОНЕЦ ВСТАВКИ ===

static struct fuse_operations users_oper = {
    .getattr = users_getattr,
    .open = users_open,
    .read = users_read,
    .readdir = users_readdir,
    .mkdir = users_mkdir,
    // === ВСТАВКА: Добавление rmdir в операции ===
    .rmdir = users_rmdir,
    // === КОНЕЦ ВСТАВКИ ===
};

int start_users_vfs(const char *mount_point) {
    int pid = fork();    
    if (pid == 0) {
        char *fuse_argv[] = {
            "users_vfs",    // имя программы
            "-f",           // foreground mode
            "-s",           // single-threaded
            (char*)mount_point, // точка монтирования
            NULL
        };
       
        if (get_users_list() <= 0) {
            fprintf(stderr, "Не удалось получить список пользователей\n");
            exit(1);
        }
       
        int ret = fuse_main(4, fuse_argv, &users_oper, NULL);
       
        free_users_list();
        exit(ret);
    } else { // Родительский процесс
        vfs_pid = pid;
        printf("VFS запущена в процессе %d, монтирована в %s\n", pid, mount_point);
        return 0;
    }
}

void stop_users_vfs() {
    if (vfs_pid != -1) {
        kill(vfs_pid, SIGTERM);
        waitpid(vfs_pid, NULL, 0);
        vfs_pid = -1;
        printf("VFS остановлена\n");
    }
}

int main(int argc, char *argv[]){
	if(argc < 2){
		fprintf(stderr, "Usage: %s <mount_point>\n", argv[0]);
		return 1;
	}
	mkdir(argv[1], 0755);
	return fuse_main(argc, argv, &users_oper, NULL);
}
