#include "log.h"
#include "socket_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

char *get_iface_ip(const char *ifname)
{
#ifndef WINDOWS
    struct ifreq if_data;
    struct in_addr in;
    char *ip_str;
    int sockd;
    u_int32_t ip;
 
    /* Create a socket */
    if ((sockd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        log_fatal(NULL,"socket() err=%d",mlc_errno);
        return NULL;
    }

    /* Get IP of internal interface */
    strncpy(if_data.ifr_name, ifname, 15);
    if_data.ifr_name[15] = '\0';

    /* Get the IP address */
    if (ioctl(sockd, SIOCGIFADDR, &if_data) < 0)
    {
        log_fatal(NULL,"ioctl(): SIOCGIFADDR err=%d",mlc_errno);
		mlc_close(sockd);
        return NULL;
    }
    memcpy((void *)&ip, (void *)&if_data.ifr_addr.sa_data + 2, 4);
    in.s_addr = ip;

	mlc_close(sockd);


#endif
    return NULL;
}

int get_all_iface_name()
{
#ifndef WINDOWS
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[2048];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1)
    {
        log_fatal(NULL,"socket() err=%d",mlc_errno);
        return -1;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
    {
        log_fatal(NULL,"ioctl error err=%d",mlc_errno);
        return -1;
    }

    struct ifreq *it = ifc.ifc_req;
    const struct ifreq *const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    char szMac[64];
    int count = 0;
    for (; it != end; ++it)
    {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS , &ifr) == 0)
        if (ifr.ifr_flags & IFF_LOOPBACK)
        { // don't count loopback
            continue;
        }
        if (ioctl(sock, SIOCGIFFLAGS | SIOCGIFADDR, &ifr) == 0)
        {
            struct in_addr in;
            uint32_t ip = 0;
            char ipaddr[16] = {0,};
            memcpy((void *)&ip, (void *)&ifr.ifr_addr.sa_data + 2, 4);
            in.s_addr = ip;
            log_error(NULL,"get ifr_name %s flags=%x ip=%s", it->ifr_name, ifr.ifr_flags,inet_ntop(AF_INET, &in, ipaddr, 16));
            count++;
        }
        else
        {
            log_error(NULL,"get if info error=%d",mlc_errno);
            return -1;
        }
    }
    return count;
#else
	return 0;
#endif
}

int socket_udp_create()
{
    return socket_nonblock_create(PF_INET,SOCK_DGRAM,0);
}
int socket_tcp_create()
{
    return socket_nonblock_create(PF_INET,SOCK_STREAM,0);
}

int socket_nonblock_create(int domain,int type,int proto)
{
    int ret = -1;
    int reuse = 1;
    int fd = socket(domain, type, proto);
    if (fd < 0)
    {
        log_fatal(NULL,"create socket ret=%d err=%d", fd,mlc_errno);
        return -1;
    }

    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret)
    {
        log_fatal(NULL,"setsockopt SO_REUSEADDR ret=%d, err=%d", ret , mlc_errno);
        return -2;
    }

#ifndef WINDOWS
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    if (ret)
    {
        log_fatal(NULL,"setsockopt SO_REUSEPORT ret=%d err=%d", ret, mlc_errno);
        return -3;
    }
#endif
    if (socket_nonblocking(fd) < 0)
    {
        log_fatal(NULL,"set nonblock ret=%d err=%d", ret,mlc_errno);
        return -4;
	}

    /*int snd_len = 1024;
    ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd_len, sizeof(snd_len));
    if (ret)
    {
        log_fatal(NULL,"setsockopt SO_REUSEPORT ret=%d", ret);
        return -3;
    }*/
    
    return fd;
}



int socket_recv(int sock, char *buffer, int len)
{
    for(;;)
    {
        int ret = recv(sock, buffer, len, 0);
        if (ret < 0)
        {
            int err = mlc_errno;
            if(err == MLC_EINTR)
            {
                continue;
            }
            else if (MLC_EAGAIN == err || MLC_EWOULDBLOCK == err)
            {
                return 0;
            }
            else
            {
                return ret;
            }
        }
        else if(ret == 0)
        {
            return MLC_CONNECTION_CLOSE;
        }    
        else
        {
            return ret;
        }
    } 
    //should not be here
    assert(0 && "cannot run to here");
    return MLC_ERR;
}

int socket_send_to(int sock, char *buffer, int len,struct sockaddr *addr,socklen_t addr_len)
{
    for(;;)
    {
        int ret = sendto(sock, buffer, len, 0,addr,addr_len);
        if (ret < 0)
        {
            int err = mlc_errno;
            if(err == MLC_EINTR)
            {
                continue;
            }
            if (err == MLC_EAGAIN || err == MLC_EWOULDBLOCK)
            {
                return 0;
            }
            else
            {
                return ret;
            }
        }
        else
        {
            return ret;
        }
    } 
    assert(0 && "cannot run to here");
    return MLC_ERR;
}
int socket_send(int sock, char *buffer, int len)
{
    for(;;)
    {
        int ret = send(sock, buffer, len, 0);
        if (ret < 0)
        {
            int err = mlc_errno;
            if(err == MLC_EINTR)
            {
                continue;
            }
            if (err == MLC_EAGAIN || err == MLC_EWOULDBLOCK)
            {
                return 0;
            }
            else
            {
                return ret;
            }
        }
        else
        {
            return ret;
        }
    } 
    assert(0 && "cannot run to here");
    return MLC_ERR;
}

int socket_udp_accept(struct sockaddr *addr_listen,struct sockaddr *addr_in)
{
    int fd = socket_udp_create();
    if (fd < 0)
    {
        log_fatal(NULL,"socket create failed ret=%d",fd);
        return MLC_ERR;
    }
    int ret = bind(fd, addr_listen, sizeof(struct sockaddr));
    if (ret)
    {
        log_fatal(NULL,"bind ret=%d", ret);
        return MLC_ERR;
    }
    ret = connect(fd, addr_in, sizeof(struct sockaddr));
    if (ret)
    {
        log_fatal(NULL,"connect ret=%d", ret);
        return MLC_ERR;
    }
    return fd;
}
