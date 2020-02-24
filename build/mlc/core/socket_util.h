#ifndef _LIBCONECT_SOCKET_UTIL_H_
#define _LIBCONECT_SOCKET_UTIL_H_

#include <fcntl.h>
#include "core.h"

#ifdef WINDOWS
static inline int socket_nonblocking(int s)
{
	unsigned long on_windows = 1; 
	return ioctlsocket(s, FIONBIO, &on_windows);
}
#define mlc_close closesocket
#else
static inline int socket_nonblocking(int s)
{
	return fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
}
#define mlc_close close
#endif // WINDOWS


MLC_API char *get_iface_ip(const char *ifname);
MLC_API int get_all_iface_name();

MLC_API int socket_udp_create();
MLC_API int socket_tcp_create();
MLC_API int socket_nonblock_create(int domain,int type,int proto);

MLC_API int socket_recv(int sock, char *buffer, int len);
MLC_API int socket_send(int sock, char *buffer, int len);
MLC_API int socket_send_to(int sock, char *buffer, int len,struct sockaddr *addr,socklen_t addr_len);
MLC_API int socket_udp_accept(struct sockaddr *addr_listen,struct sockaddr *addr_in);

#endif