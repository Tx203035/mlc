#ifndef _WINDOWS_H_INCLUDED_
#define _WINDOWS_H_INCLUDED_

#define WIN32_LEAN_AND_MEAN

#define STDIN_LINENO 0 
#define STDIN_FILENO 0
#define MLC_USE_SELECT

#include <windows.h>

#include <winsock2.h>
#include <ws2tcpip.h>  /* ipv6 */
#include <mswsock.h>
#include <shellapi.h>
#include <stddef.h>    /* offsetof() */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <locale.h>

#include <io.h>

#include <time.h>      /* localtime(), strftime() */

#include <assert.h>

#endif