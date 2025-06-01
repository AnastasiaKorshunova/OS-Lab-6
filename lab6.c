#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>

#define NUM_CHILDREN 5
#define LOG_FILE "log.txt"
#define BUFSIZE 1024

void get_time(char *buffer, size_t size);
int count_previous_launches_all(int counts[], const char *greetings[], int n);
int check_greeting_line(const char *line, const char *greeting);
void write_log(const char *greeting, int launch_number);

const char *greetings[NUM_CHILDREN] = {
    "Hello from child 1",
    "Hey from child 2",
    "Hi from child 3",
    "Labas from child 4",
    "Good evening from child 5"
};

void get_time(char *buffer, size_t size) {
    time_t raw_time;
    struct tm *timeinfo;
    time(&raw_time);
    timeinfo = localtime(&raw_time);
    strftime(buffer, size, "%Y-%m-%dT%H:%M:%S%z", timeinfo);
}

int check_greeting_line(const char *line, const char *greeting) {
    const char *first = strchr(line, '|');
    if (!first) return 0;
    const char *second = strchr(first + 1, '|');
    if (!second) return 0;
    const char *greet_start = first + 1;
    while (*greet_start == ' ') greet_start++;
    const char *greet_end = second;
    while (greet_end > greet_start && *(greet_end - 1) == ' ') greet_end--;
    size_t len = greet_end - greet_start;
    char extracted[128];
    if (len >= sizeof(extracted)) return 0;
    strncpy(extracted, greet_start, len);
    extracted[len] = '\0';
    return strcmp(extracted, greeting) == 0;
}

int count_previous_launches_all(int counts[], const char *greetings[], int n) {
    for (int i = 0; i < n; i++) counts[i] = 0;
    if (access(LOG_FILE, F_OK) != 0) return 0;
    int fd = open(LOG_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open for reading failed");
        return 0;
    }
    char buffer[BUFSIZE];
    char line[256];
    ssize_t bytes_read;
    int line_pos = 0;
    while ((bytes_read = read(fd, buffer, BUFSIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n' || line_pos >= 255) {
                line[line_pos] = '\0';
                for (int j = 0; j < n; j++) {
                    if (check_greeting_line(line, greetings[j])) {
                        counts[j]++;
                        break;
                    }
                }
                line_pos = 0;
            } else {
                line[line_pos++] = c;
            }
        }
    }
    close(fd);
    return 1;
}

void write_log(const char *greeting, int launch_number) {
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        perror("open failed");
        return;
    }

    if (flock(fd, LOCK_EX) < 0) {
        perror("flock failed");
        close(fd);
        return;
    }

    if (lseek(fd, 0, SEEK_END) < 0) {
        perror("lseek failed");
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }

    char time_str[32];
    get_time(time_str, sizeof(time_str));

    char entry[256];
    snprintf(entry, sizeof(entry), "PID: %d | %s | %s | Launch #%d\n", getpid(), greeting, time_str, launch_number);

    if (write(fd, entry, strlen(entry)) < 0) {
        perror("write failed");
    }

    flock(fd, LOCK_UN);
    close(fd);
}

int main() {
    int launch_counts[NUM_CHILDREN];
    count_previous_launches_all(launch_counts, greetings, NUM_CHILDREN);
    for (int i = 0; i < NUM_CHILDREN; i++) launch_counts[i]++;
    pid_t pids[NUM_CHILDREN];

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            const char *greeting = greetings[i];
            int launch = launch_counts[i];
            char time_str[32];
            get_time(time_str, sizeof(time_str));
            printf("PID: %d | %s | %s | Launch #%d\n", getpid(), greeting, time_str, launch);
            fflush(stdout);
            write_log(greeting, launch);
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
    usleep(500000);

    char screenshot_name[64];
    snprintf(screenshot_name, sizeof(screenshot_name), "screenshot_launch_%d.png", launch_counts[0]);

    pid_t scrot_pid = fork();
    if (scrot_pid == 0) {
        char *args[] = {"scrot", screenshot_name, NULL};
        execvp("scrot", args);
        perror("scrot failed");
        exit(EXIT_FAILURE);
    } else if (scrot_pid > 0) {
        waitpid(scrot_pid, NULL, 0);
        printf("Screenshot saved as %s\n", screenshot_name);
    }

    return 0;
}
