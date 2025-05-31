#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define NUM_CHILDREN 5
#define LOG_FILE "log.txt"
#define BUFSIZE 1024

void get_iso_time(char *buffer, size_t size);
int count_previous_launches(const char *greeting);
void write_log_entry(const char *entry);
void handle_sigint(int sig);

const char *greetings[NUM_CHILDREN] = {
    "Hello from child 1",
    "Greetings from child 2",
    "Hi there from child 3",
    "Salutations from child 4",
    "Hey! It's child 5"
};

void handle_sigint(int sig) {
    (void)sig; // suppress unused warning
    write(STDOUT_FILENO, "\nSIGINT received. Exiting cleanly.\n", 35);
    _exit(0); // немедленный выход, безопасный в signal handler
}

void get_iso_time(char *buffer, size_t size) {
    time_t raw_time;
    struct tm *timeinfo;
    time(&raw_time);
    timeinfo = localtime(&raw_time);
    strftime(buffer, size, "%Y-%m-%dT%H:%M:%S%z", timeinfo);
}

int count_previous_launches(const char *greeting) {
    int fd = open(LOG_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) {
        perror("open for reading failed");
        return 0;
    }

    char buffer[BUFSIZE + 1];
    char line[256];
    int count = 0;
    ssize_t bytes_read;
    int line_pos = 0;

    while ((bytes_read = read(fd, buffer, BUFSIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n' || line_pos >= 255) {
                line[line_pos] = '\0';

        
                char *first = strchr(line, '|');
                if (first) {
                    char *second = strchr(first + 1, '|');
                    if (second) {
                        second += 1;  |
                        while (*second == ' ') second++; 

                       
                        char temp[128];
                        int j = 0;
                        while (*second && *second != '|' && j < 127) {
                            temp[j++] = *second++;
                        }
                        temp[j] = '\0';

                        if (strcmp(temp, greeting) == 0) {
                            count++;
                        }
                    }
                }

                line_pos = 0;
            } else {
                line[line_pos++] = c;
            }
        }
    }

    close(fd);
    return count;
}



void write_log_entry(const char *entry) {
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open for writing failed");
        return;
    }

    ssize_t written = write(fd, entry, strlen(entry));
    if (written < 0) {
        perror("write failed");
    }

    close(fd);
}


  int main() {
    signal(SIGINT, handle_sigint);

    pid_t pids[NUM_CHILDREN];

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            const char *greeting = greetings[i];
            pid_t my_pid = getpid();

            char time_str[32];
            get_iso_time(time_str, sizeof(time_str));

            int count = count_previous_launches(greeting) + 1;

            char log_entry[256];
            snprintf(log_entry, sizeof(log_entry),
                     "PID: %d | %s | %s | Launch #%d\n",
                     my_pid, greeting, time_str, count);

            printf("%s", log_entry);

            fflush(stdout);  

            write_log_entry(log_entry);
            exit(EXIT_SUCCESS);
        } else {
            pids[i] = pid;
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        waitpid(pids[i], NULL, 0);
    }

    printf("Parent process completed.\n");

    fflush(stdout);

    usleep(300000);  

    pid_t scrot_pid = fork();

    if (scrot_pid == 0) {
        char *args[] = {"scrot", "screenshot_log.png", NULL};
        execvp("scrot", args);
        perror("exec failed for scrot");
        exit(EXIT_FAILURE);
    } else if (scrot_pid > 0) {
        waitpid(scrot_pid, NULL, 0);
        printf("Screenshot taken as screenshot_log.png\n");
    } else {
        perror("fork failed for scrot");
    }

    return 0;
}