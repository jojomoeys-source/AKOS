#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void printUsage(const char *programName) {
    printf("Usage: %s [--host ADDRESS] [--port PORT]\n", programName);
    printf("Defaults: --host 127.0.0.1 --port 5050\n");
}

static int connectToServer(const char *host, const char *port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *addresses = NULL;
    int result = getaddrinfo(host, port, &hints, &addresses);
    if (result != 0) {
        fprintf(stderr, "Server address: %s\n", gai_strerror(result));
        return -1;
    }

    int descriptor = -1;
    for (struct addrinfo *entry = addresses; entry != NULL; entry = entry->ai_next) {
        descriptor = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (descriptor >= 0 && connect(descriptor, entry->ai_addr, entry->ai_addrlen) == 0) {
            break;
        }
        if (descriptor >= 0) {
            close(descriptor);
            descriptor = -1;
        }
    }
    freeaddrinfo(addresses);
    return descriptor;
}

int main(int argumentCount, char **argumentValues) {
    // Клиент печатает полученные события
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal SIGPIPE");
        return 1;
    }

    const char *host = "127.0.0.1";
    const char *port = "5050";

    for (int i = 1; i < argumentCount; i++) {
        if (strcmp(argumentValues[i], "--help") == 0) {
            printUsage(argumentValues[0]);
            return 0;
        }
        if ((strcmp(argumentValues[i], "--host") == 0 ||
             strcmp(argumentValues[i], "--port") == 0) && i + 1 < argumentCount) {
            if (strcmp(argumentValues[i], "--host") == 0) {
                host = argumentValues[++i];
            } else {
                port = argumentValues[++i];
            }
            continue;
        }
        fprintf(stderr, "Invalid option: %s\n", argumentValues[i]);
        printUsage(argumentValues[0]);
        return 1;
    }

    int server = connectToServer(host, port);
    if (server == -1) {
        perror("connect elevator server");
        return 1;
    }

    static const char observerRole[] = "OBSERVER\n";
    size_t roleSent = 0;
    while (roleSent < sizeof(observerRole) - 1) {
#ifdef MSG_NOSIGNAL
        ssize_t count = send(server, observerRole + roleSent,
                             sizeof(observerRole) - 1 - roleSent, MSG_NOSIGNAL);
#else
        ssize_t count = send(server, observerRole + roleSent,
                             sizeof(observerRole) - 1 - roleSent, 0);
#endif
        if (count == -1 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            perror("identify elevator observer");
            close(server);
            return 1;
        }
        roleSent += (size_t)count;
    }

    char buffer[4096];
    for (;;) {
        ssize_t received = recv(server, buffer, sizeof(buffer), 0);
        if (received == 0) {
            break;
        }
        if (received == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv elevator events");
            close(server);
            return 1;
        }

        size_t written = 0;
        while (written < (size_t)received) {
            ssize_t count = write(STDOUT_FILENO, buffer + written,
                                  (size_t)received - written);
            if (count == -1 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                perror("write observer output");
                close(server);
                return 1;
            }
            written += (size_t)count;
        }
    }

    close(server);
    return 0;
}
