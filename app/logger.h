#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

typedef enum {
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL
} LogLevel;

void logger_init(const char *filename, LogLevel level);
void logger_set_level(LogLevel level);
void logger_log(LogLevel level, const char *file, int line, const char *fmt,
                ...);
void logger_close(void);

#define LOG_TRACE(...) logger_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) logger_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) logger_log(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) logger_log(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) logger_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
