/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */


#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include "export.h"

#define LOG_VERSION "0.1.0"


#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

typedef void(*log_output_function)(const char *msg);
typedef struct logger_s{
  int level;
  void *udata;
  FILE *fp;
  int quiet;
  log_output_function output;
} logger_t;

extern logger_t glogger;

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(l,...) log_log(l,LOG_TRACE, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_debug(l,...) log_log(l,LOG_DEBUG, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_info(l,...)  log_log(l,LOG_INFO,  __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_warn(l,...)  log_log(l,LOG_WARN,  __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_error(l,...) log_log(l,LOG_ERROR, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_fatal(l,...) log_log(l,LOG_FATAL, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_errno(l,fmt,...) log_error(l,"errno=%d(%s)|" fmt,mlc_errno,mlc_strerror(mlc_errno),__VA_ARGS__)

static inline void log_set_udata(logger_t *l,void *udata)
{
    l->udata = udata;
}
static inline void log_set_fp(logger_t *l,FILE *fp)
{
    l->fp = fp;
}

static inline void log_set_level(logger_t *l,int level)
{
    l->level = level;
}

static inline void log_set_quiet(logger_t *l,int enable)
{
    l->quiet = enable ? 1 : 0;
}

static inline void log_set_output(logger_t *l,log_output_function output)
{
    l->output = output;
}

MLC_API void log_log(logger_t *l, int level, const char *file, int line, const char *fmt, ...);

#endif
