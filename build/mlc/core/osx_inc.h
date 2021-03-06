
#ifndef _MAC_OS_H_INCLUDED_
#define _MAC_OS_H_INCLUDED_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* pread(), pwrite(), gethostname() */
#endif

#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h> /* offsetof() */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <glob.h>
// #include <sys/vfs.h> /* statfs() */

#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sched.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* TCP_NODELAY, TCP_CORK */
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_dl.h>

#include <time.h>   /* tzset() */
// #include <malloc.h> /* memalign() */
#include <limits.h> /* IOV_MAX */
#include <sys/ioctl.h>
// #include <crypt.h>
#include <sys/utsname.h> /* uname() */
#include <dlfcn.h>

#include <sys/syscall.h>
#include <assert.h>


#define MLC_USE_KQUEUE 1


#endif /* _LINUX_H_INCLUDED_ */
