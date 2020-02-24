/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "util.h"

logger_t glogger = {LOG_WARN,NULL,0,0,NULL};

static const char *level_names[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif



void log_log(logger_t *l , int level, const char *file, int line, const char *fmt, ...) {
  l = l ? l : &glogger;
  if (level < l->level) {
    return;
  }

  /* Get current time */
  // time_t t = time(NULL);
  uint64_t ms = mlc_clock64_ms();
  time_t t = ms / 1000;
  int now_ms = ms % 1000;
  struct tm *lt = localtime(&t);

  /* Log to stdout */
  if (!l->quiet) {
    va_list args;
    char buf[32];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
#ifdef LOG_USE_COLOR
    fprintf(
      stdout, "%s:%03d %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
      buf, now_ms, level_colors[level], level_names[level], file, line);
#else
    fprintf(stdout, "%s:%03d %-5s %s:%d: ", buf,now_ms, level_names[level], file, line);
#endif
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    if(l->output)
    {
        char buffer_fmt[100];
        char buffer[10*1024];
        sprintf(buffer_fmt,"%s:%03d %-5s %s:%d: %s\n", buf,now_ms, level_names[level], file, line,fmt);
        sprintf(buffer,buffer_fmt,args);
        l->output(buffer);
    }    
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);
  }

  /* Log to file */
  if (l->fp) {
    va_list args;
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
    fprintf(l->fp, "%s:%03d %-5s %s:%d: ", buf,now_ms, level_names[level], file, line);
    va_start(args, fmt);
    vfprintf(l->fp, fmt, args);
    va_end(args);
    fprintf(l->fp, "\n");
    fflush(l->fp);
  }

}
