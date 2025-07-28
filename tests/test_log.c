/* Simple logging implementation for daemon tests */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

// Log levels
enum {
    GOX_LOG_VERBOSE = 2,
    GOX_LOG_DEBUG   = 3,
    GOX_LOG_INFO    = 4,
    GOX_LOG_WARN    = 5,
    GOX_LOG_ERROR   = 6,
};

static const char *level_names[] = {
    [GOX_LOG_VERBOSE] = "VERBOSE",
    [GOX_LOG_DEBUG]   = "DEBUG",
    [GOX_LOG_INFO]    = "INFO",
    [GOX_LOG_WARN]    = "WARN",
    [GOX_LOG_ERROR]   = "ERROR",
};

void dolog(int level, const char *msg, const char *func, const char *file, int line, ...)
{
    va_list args;
    time_t now;
    struct tm *timeinfo;
    char timestamp[32];
    const char *filename;
    
    // Get timestamp
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    
    // Get just the filename without path
    filename = strrchr(file, '/');
    filename = filename ? filename + 1 : file;
    
    // Print log message
    printf("[%s] %s %s:%d %s() - ", 
           timestamp,
           level < GOX_LOG_VERBOSE || level > GOX_LOG_ERROR ? "UNKNOWN" : level_names[level],
           filename, line, func);
    
    va_start(args, line);
    vprintf(msg, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
}