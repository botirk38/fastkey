#include "logger.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *log_file = NULL;
static LogLevel current_level = LOG_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *level_strings[] = {"TRACE", "DEBUG", "INFO",
                                      "WARN",  "ERROR", "FATAL"};

static const char *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m",
                                     "\x1b[33m", "\x1b[31m", "\x1b[35m"};

void logger_init(const char *filename, LogLevel level) {
  if (filename) {
    log_file = fopen(filename, "a");
    if (!log_file) {
      fprintf(stderr, "Failed to open log file: %s\n", filename);
      return;
    }
  }
  current_level = level;
}

void logger_set_level(LogLevel level) { current_level = level; }

void logger_log(LogLevel level, const char *file, int line, const char *fmt,
                ...) {
  if (level < current_level)
    return;

  pthread_mutex_lock(&log_mutex);

  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  char timestamp[20];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", lt);

  // Write to stdout with colors
  printf("%s%s %-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ", level_colors[level],
         timestamp, level_strings[level], file, line);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);

  // Write to file if opened
  if (log_file) {
    fprintf(log_file, "%s %-5s %s:%d: ", timestamp, level_strings[level], file,
            line);
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    va_end(args);
    fflush(log_file);
  }

  pthread_mutex_unlock(&log_mutex);
}

void logger_close(void) {
  if (log_file) {
    fclose(log_file);
    log_file = NULL;
  }
}
