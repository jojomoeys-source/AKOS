#ifndef TCP_EVENTS_H
#define TCP_EVENTS_H

int startTcpEventServer(const char *bindAddress, const char *port);
int pumpTcpEvents(void);
void stopTcpEventServer(void);

#endif
