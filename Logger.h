#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <string>

// Log severity levels
enum LOG_LEVEL {
    DEBUG = 0,
    WARNING = 1,
    ERROR = 2,
    CRITICAL = 3
};

// Logger functions
int InitializeLog();
void SetLogLevel(LOG_LEVEL level);
void Log(LOG_LEVEL level, const char *prog, const char *func, int line, const char *message);
void ExitLog();

#endif // LOGGER_H
