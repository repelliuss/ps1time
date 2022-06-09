#pragma once

#include "log.hpp"

// 0 is success

#define RETONERR(expr, msg)                                                    \
  do {                                                                         \
    const int status___ = (expr);                                              \
    if (status___) {                                                           \
      LOG_ERROR((msg));                                                        \
      return status___;                                                        \
    }                                                                          \
  } while (0)

#define BRETONERR(expr, ...)                                                   \
  do {                                                                         \
    const int status___ = (expr);                                              \
    if (status___) {                                                           \
      {__VA_ARGS__};                                                           \
      return status___;                                                        \
    }                                                                          \
  } while (0)

#define QRETONERR(expr)                                                        \
  do {                                                                         \
    const int status___ = (expr);                                              \
    if (status___) {                                                           \
      return status___;                                                        \
    }                                                                          \
  } while (0)
