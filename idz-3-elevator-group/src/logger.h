#ifndef LOGGER_H
#define LOGGER_H

int openLog(const char *filePath);
int openLogAppend(const char *filePath);
void logEvent(const char *format, ...);
void closeLog(void);

#endif
