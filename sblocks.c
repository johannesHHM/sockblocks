#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <time.h>

#include <X11/Xlib.h>

#define SOCKET_PATH "/tmp/dwm-bar.sock"
#define MAX_CMDLEN 100
#define MAX_BACKLOG 10

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))
#define MAX_BARLEN (LENGTH(blocks) * MAX_CMDLEN) + (sizeof(seperator) * (LENGTH(blocks) - 1)) + sizeof(left) + sizeof(right) + 1
pthread_mutex_t block_mutex = PTHREAD_MUTEX_INITIALIZER;
static int denominator = 0;
static Display *display;
static Window root;

typedef struct {
    char *head;
    char *command;
    int period;
    int signal;
} Block;

#include "config.h"

char bar[LENGTH(blocks)][MAX_CMDLEN] = {0};
char barbuf[MAX_BARLEN];

void printbar(void);
void runcmd(const Block *block, char *out);
void *timerthread(void* arg);
void *socketthread(void* arg);
void die(char *msg);

void die(char *msg) {
    fprintf(stderr, "ERROR: ");
    if (errno)
        perror(msg);
    else
        fprintf(stderr, "%s\n", msg);
    exit(1);
}

void printbar(void) {
    barbuf[0] = '\0';
    strcat(barbuf, left);
    for (size_t i = 0; i < LENGTH(blocks); i++) {
        strlcat(barbuf, bar[i], MAX_BARLEN);
        if (i < LENGTH(blocks) - 1)
            strcat(barbuf, seperator);
    }
    strcat(barbuf, right);
    XStoreName(display, root, barbuf);
    XFlush(display);
}

void runcmd(const Block *block, char *out) {
    char temp[MAX_CMDLEN] = {0};
    int signudge = 0;
    if (block->signal) {
        temp[0] = (char)block->signal;
        signudge = 1;
    }
    strlcpy(temp + signudge, block->head, MAX_CMDLEN - 1);
    unsigned int headlen = strlen(block->head) + signudge;

    FILE *cmdf = popen(block->command, "r");
    if (!cmdf) {
        strcpy(out, "ERROR");
        return;
    }
    
    char *s = fgets(temp + headlen, MAX_CMDLEN - headlen, cmdf);
    if (s) {
        int i = strlen(temp);
        if (temp[i - 1] == '\n')
            temp[i - 1] = '\0';
        strlcpy(out, temp, MAX_CMDLEN);
    }
    else
        strcpy(out, "ERROR");
    pclose(cmdf);
}

int gcd(int a, int b) {
    int temp;
    while (b > 0) {
        temp = a % b;
        a = b;
        b = temp;
    }
    return a;
}

void* timerthread(void* arg) {
    static int timer = 0;
    while (1) {
        pthread_mutex_lock(&block_mutex);
        
        for (int i = 0; i < LENGTH(blocks); i++)
            if ((blocks[i].period != 0 && timer % blocks[i].period == 0) || timer == 0)
                runcmd(&blocks[i], bar[i]);

        pthread_mutex_unlock(&block_mutex);
        
        printbar();

        sleep(denominator);
        timer += denominator;
    }

    return NULL;
}

void* handleclient(void* arg) {
    int client_sock = *((int*)arg);
    char buffer[256];

    ssize_t len = read(client_sock, buffer, sizeof(buffer) - 1);
    if (len <= 0)
        goto CLOSE;

    buffer[len] = '\0';
    int signal, button = 0, blocknum = -1;
    
    if (sscanf(buffer, "%d %d", &signal, &button) < 1)
        goto CLOSE;

    if (signal == 0) {
        pthread_mutex_lock(&block_mutex);
        for (int i = 0; i < LENGTH(blocks); i++)
            runcmd(&blocks[i], bar[i]);
        pthread_mutex_unlock(&block_mutex);
        
        printbar();
        goto CLOSE;
    }
    
    for (int i = 0; i < LENGTH(blocks); i++)
        if (blocks[i].signal == signal)
            blocknum = i;
    if (blocknum == -1)
        goto CLOSE;

    pthread_mutex_lock(&block_mutex);
    if (button) {
        char buttonstr[2] = { '0' + button, '\0'};
        setenv("BUTTON", buttonstr, 1);
    }
    runcmd(&blocks[blocknum], bar[blocknum]);
    unsetenv("BUTTON");
    pthread_mutex_unlock(&block_mutex);

    printbar();

CLOSE:
    close(client_sock);
    return NULL;
}

void* socketthread(void* arg) {
    int server_sock, client_sock;
    struct sockaddr_un addr;

    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT)
        die("Failed to remove existing socket");

    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock == -1)
        die("Failed to create socket");

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1)
        die("Binding to socket failed");

    if (listen(server_sock, MAX_BACKLOG) == -1)
        die("Listen to socket failed");

    while (1) {
        client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == -1) {
            perror("ERROR: Accept failed");
            continue;
        }

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handleclient, (void*)&client_sock) != 0) {
            perror("ERROR: Failed to create thread for client");
            close(client_sock);
        } else {
            pthread_detach(client_thread);
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    display = XOpenDisplay(NULL);
    if (display == NULL)
        die("Failed to open X display");
    root = DefaultRootWindow(display);

    for (int i = 0; i < LENGTH(blocks); i++)
        if (blocks[i].period)
            denominator = gcd(blocks[i].period, denominator);

    pthread_t timertid, sockettid;
    errno = 0;
    
    if (pthread_create(&timertid, NULL, timerthread, NULL) != 0)
        die("Failed to create timer thread");

    if (pthread_create(&sockettid, NULL, socketthread, NULL) != 0)
        die("Failed to create socket thread");

    pthread_join(timertid, NULL);
    pthread_join(sockettid, NULL);
    XCloseDisplay(display);

    return 0;
}

