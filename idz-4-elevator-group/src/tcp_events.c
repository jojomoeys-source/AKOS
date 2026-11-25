#define _POSIX_C_SOURCE 200809L

#include "tcp_events.h"

#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_MONITORS 64
#define EVENT_BUFFER_SIZE 4096

// Дескрипторы передачи событий
static int eventReadDescriptor = -1;
static int eventWriteDescriptor = -1;
static int publisherDescriptor = -1;
static pid_t eventServerPid = -1;

static int sendAll(int descriptor, const char *data, size_t length) {
    // Отправка всего буфера
    size_t sent = 0;
    while (sent < length) {
#ifdef MSG_NOSIGNAL
        ssize_t result = send(
            descriptor,
            data + sent,
            length - sent,
            MSG_NOSIGNAL
        );
#else
        ssize_t result = send(descriptor, data + sent, length - sent, 0);
#endif
        if (result == -1 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }
        sent += (size_t)result;
    }
    return 0;
}

static int readConnectionRole(int descriptor, char role[16]) {
    // Тип соединения задаёт строка SOURCE или OBSERVER
    size_t used = 0;
    while (used < 15) {
        ssize_t count = recv(descriptor, role + used, 1, 0);
        if (count == -1 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return -1;
        }
        if (role[used++] == '\n') {
            role[used] = '\0';
            return 0;
        }
    }
    return -1;
}

static int createListener(const char *address, const char *port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *addresses = NULL;
    int result = getaddrinfo(address, port, &hints, &addresses);
    if (result != 0) {
        fprintf(stderr, "TCP server address: %s\n", gai_strerror(result));
        return -1;
    }

    int listener = -1;
    for (struct addrinfo *entry = addresses; entry != NULL; entry = entry->ai_next) {
        listener = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (listener == -1) {
            continue;
        }

        int reuseAddress = 1;
        (void)setsockopt(
            listener,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)
        );

        if (bind(listener, entry->ai_addr, entry->ai_addrlen) == 0 &&
            listen(listener, MAX_MONITORS + 1) == 0) {
            break;
        }

        close(listener);
        listener = -1;
    }

    freeaddrinfo(addresses);
    return listener;
}

static void closeMonitors(int monitors[], int monitorCount) {
    for (int i = 0; i < monitorCount; i++) {
        if (monitors[i] >= 0) {
            close(monitors[i]);
        }
    }
}

static int runEventServer(
    const char *address,
    const char *port,
    int readinessDescriptor
) {
    // Приём событий и подключений в одном цикле poll
    signal(SIGPIPE, SIG_IGN);
    int listener = createListener(address, port);
    if (listener == -1) {
        char failed = 'E';
        (void)write(readinessDescriptor, &failed, sizeof(failed));
        close(readinessDescriptor);
        return -1;
    }

    char ready = 'R';
    if (write(readinessDescriptor, &ready, sizeof(ready)) != sizeof(ready)) {
        close(listener);
        close(readinessDescriptor);
        return -1;
    }
    close(readinessDescriptor);

    int publisher = -1;
    int monitors[MAX_MONITORS];
    for (int i = 0; i < MAX_MONITORS; i++) {
        monitors[i] = -1;
    }
    char buffer[EVENT_BUFFER_SIZE];
    int running = 1;

    while (running) {
        struct pollfd descriptors[MAX_MONITORS + 2];
        int monitorIndexes[MAX_MONITORS + 2];
        nfds_t count = 0;

        descriptors[count] = (struct pollfd){.fd = listener, .events = POLLIN};
        monitorIndexes[count++] = -1;
        if (publisher >= 0) {
            descriptors[count] = (struct pollfd){.fd = publisher, .events = POLLIN};
            monitorIndexes[count++] = -2;
        }
        for (int i = 0; i < MAX_MONITORS; i++) {
            if (monitors[i] >= 0) {
                descriptors[count] = (struct pollfd){.fd = monitors[i], .events = 0};
                monitorIndexes[count++] = i;
            }
        }

        int pollResult;
        do {
            pollResult = poll(descriptors, count, -1);
        } while (pollResult == -1 && errno == EINTR);
        if (pollResult == -1) {
            break;
        }

        for (nfds_t i = 0; i < count && running; i++) {
            if (descriptors[i].revents == 0) {
                continue;
            }

            if (monitorIndexes[i] == -1) {
                int connection = accept(listener, NULL, NULL);
                if (connection == -1) {
                    continue;
                }
                char role[16];
                if (readConnectionRole(connection, role) == -1) {
                    close(connection);
                    continue;
                }
                if (strcmp(role, "SOURCE\n") == 0 && publisher == -1) {
                    publisher = connection;
                    continue;
                }
                if (strcmp(role, "OBSERVER\n") != 0) {
                    close(connection);
                    continue;
                }

                int slot = -1;
                for (int j = 0; j < MAX_MONITORS; j++) {
                    if (monitors[j] == -1) {
                        slot = j;
                        break;
                    }
                }
                static const char greeting[] =
                    "Connected to the elevator event stream.\n";
                if (slot == -1 || sendAll(
                        connection,
                        greeting,
                        sizeof(greeting) - 1) == -1) {
                    close(connection);
                } else {
                    monitors[slot] = connection;
                }
                continue;
            }

            if (monitorIndexes[i] == -2) {
                ssize_t received = recv(publisher, buffer, sizeof(buffer), 0);
                if (received <= 0) {
                    running = 0;
                    continue;
                }
                for (int j = 0; j < MAX_MONITORS; j++) {
                    if (monitors[j] >= 0 && sendAll(
                            monitors[j], buffer, (size_t)received) == -1) {
                        close(monitors[j]);
                        monitors[j] = -1;
                    }
                }
                continue;
            }

            int slot = monitorIndexes[i];
            char discarded;
            ssize_t received = recv(monitors[slot], &discarded, sizeof(discarded), 0);
            if (received <= 0) {
                close(monitors[slot]);
                monitors[slot] = -1;
            }
        }
    }

    static const char finishedMessage[] =
        "\n[Server] Simulation finished; observer connections are closing.\n";
    for (int i = 0; i < MAX_MONITORS; i++) {
        if (monitors[i] >= 0) {
            (void)sendAll(
                monitors[i],
                finishedMessage,
                sizeof(finishedMessage) - 1
            );
        }
    }
    closeMonitors(monitors, MAX_MONITORS);
    if (publisher >= 0) {
        close(publisher);
    }
    close(listener);
    return running ? -1 : 0;
}

static int connectPublisher(const char *address, const char *port) {
    const char *connectAddress = address;
    if (address == NULL || address[0] == '\0' || strcmp(address, "0.0.0.0") == 0) {
        connectAddress = "127.0.0.1";
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *addresses = NULL;
    int result = getaddrinfo(connectAddress, port, &hints, &addresses);
    if (result != 0) {
        fprintf(stderr, "TCP publisher address: %s\n", gai_strerror(result));
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

int startTcpEventServer(const char *bindAddress, const char *port) {
    if (eventServerPid > 0 || bindAddress == NULL || port == NULL) {
        return -1;
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal SIGPIPE");
        return -1;
    }

    // Запуск TCP-сервера до старта модели
    int readyPipe[2];
    if (pipe(readyPipe) == -1) {
        perror("pipe TCP server readiness");
        return -1;
    }

    pid_t child = fork();
    if (child == -1) {
        perror("fork TCP server");
        close(readyPipe[0]);
        close(readyPipe[1]);
        return -1;
    }
    if (child == 0) {
        close(readyPipe[0]);
        int result = runEventServer(bindAddress, port, readyPipe[1]);
        _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    eventServerPid = child;
    close(readyPipe[1]);
    char ready = 0;
    ssize_t count;
    do {
        count = read(readyPipe[0], &ready, sizeof(ready));
    } while (count == -1 && errno == EINTR);
    close(readyPipe[0]);
    if (count != sizeof(ready) || ready != 'R') {
        (void)waitpid(eventServerPid, NULL, 0);
        eventServerPid = -1;
        return -1;
    }

    publisherDescriptor = connectPublisher(bindAddress, port);
    if (publisherDescriptor == -1) {
        perror("connect TCP event server");
        stopTcpEventServer();
        return -1;
    }
    if (sendAll(publisherDescriptor, "SOURCE\n", 7) == -1) {
        perror("identify TCP event publisher");
        stopTcpEventServer();
        return -1;
    }

    int eventPipe[2];
    if (pipe(eventPipe) == -1) {
        perror("pipe TCP events");
        stopTcpEventServer();
        return -1;
    }
    int flags = fcntl(eventPipe[0], F_GETFL, 0);
    if (flags == -1 || fcntl(eventPipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl TCP event pipe");
        close(eventPipe[0]);
        close(eventPipe[1]);
        stopTcpEventServer();
        return -1;
    }
    eventReadDescriptor = eventPipe[0];
    eventWriteDescriptor = eventPipe[1];
    setEventPipe(eventWriteDescriptor);
    return 0;
}

int pumpTcpEvents(void) {
    if (eventReadDescriptor == -1 || publisherDescriptor == -1) {
        return 0;
    }

    // Передача очереди событий TCP-серверу
    char buffer[EVENT_BUFFER_SIZE];
    for (;;) {
        ssize_t count = read(eventReadDescriptor, buffer, sizeof(buffer));
        if (count > 0) {
            if (sendAll(publisherDescriptor, buffer, (size_t)count) == -1) {
                perror("send TCP events");
                return -1;
            }
            continue;
        }
        if (count == -1 && errno == EINTR) {
            continue;
        }
        if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }
        return count == 0 ? 0 : -1;
    }
}

void stopTcpEventServer(void) {
    // Завершение источника событий и TCP-сервера
    setEventPipe(-1);
    if (eventWriteDescriptor >= 0) {
        close(eventWriteDescriptor);
        eventWriteDescriptor = -1;
    }

    if (eventReadDescriptor >= 0) {
        char buffer[EVENT_BUFFER_SIZE];
        for (;;) {
            ssize_t count = read(eventReadDescriptor, buffer, sizeof(buffer));
            if (count > 0) {
                if (sendAll(
                        publisherDescriptor,
                        buffer,
                        (size_t)count) == -1) {
                    break;
                }
                continue;
            }
            if (count == -1 && errno == EINTR) {
                continue;
            }
            if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            if (count == -1) {
                break;
            }
            break;
        }
        close(eventReadDescriptor);
        eventReadDescriptor = -1;
    }

    if (publisherDescriptor >= 0) {
        shutdown(publisherDescriptor, SHUT_WR);
        close(publisherDescriptor);
        publisherDescriptor = -1;
    }

    if (eventServerPid > 0) {
        while (waitpid(eventServerPid, NULL, 0) == -1 && errno == EINTR) {
        }
        eventServerPid = -1;
    }
}
