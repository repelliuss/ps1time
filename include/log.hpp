#pragma once

#include <stdio.h>
#include <string.h>

#define LOG_DEPTH_CRITICAL 10
#define LOG_DEPTH_ERROR 20
#define LOG_DEPTH_WARN 30
#define LOG_DEPTH_INFO 40
#define LOG_DEPTH_DEBUG 50
#define LOG_DEPTH_TRACE 60
#define LOG_DEPTH_MAX 1000

#ifndef LOG_CURRENT_DEPTH
#define LOG_CURRENT_DEPTH LOG_DEPTH_INFO
#endif

struct LogInfo {
  const char *level;
  const char *file;
  const int line;
};

constexpr void log_metadata(const LogInfo &info) {
  printf("[%s] [%s:%d] ", info.level, info.file, info.line);
}

template <typename... Rest>
constexpr void log(const LogInfo &&info, const char *fmt,
                   Rest &&...args) {
  log_metadata(info);
  printf(fmt, args...);
  printf("\n");
}

constexpr void log(const LogInfo &&info, const char *fmt) {
  log_metadata(info);
  printf("%s\n", fmt);
}

#define LOG_LEVEL(level, ...)						\
  log(LogInfo{level, (strrchr("/" __FILE__, '/') + 1), __LINE__}, __VA_ARGS__)

#if LOG_CURRENT_DEPTH >= LOG_DEPTH_INFO
#define LOG_INFO(...) LOG_LEVEL("info", __VA_ARGS__)
#else
#define LOG_INFO(...) (void)0
#endif

#if LOG_CURRENT_DEPTH >= LOG_DEPTH_DEBUG
#define LOG_DEBUG(...) LOG_LEVEL("debug", __VA_ARGS__)
#else
#define LOG_DEBUG(...) (void)0
#endif

#if LOG_CURRENT_DEPTH >= LOG_DEPTH_ERROR
#define LOG_ERROR(...) LOG_LEVEL("error", __VA_ARGS__)
#else
#define LOG_ERROR(...) (void)0
#endif

