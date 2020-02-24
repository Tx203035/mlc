#ifndef _LIB_MLC_ERR_CODE_H_
#define _LIB_MLC_ERR_CODE_H_


#ifdef WINDOWS

#define mlc_errno WSAGetLastError()
static inline const char *mlc_strerror(int __err)
{
	return NULL;
}

#else

#define mlc_errno errno
static inline const char *mlc_strerror(int __err)
{
	return (const char *)strerror(__err);
}

#endif



enum
{
    MLC_OK = 0,
    MLC_ERR = -1,
    MLC_AGAIN = -2,
    MLC_BUSY = -3,
    MLC_CONNECTION_CLOSE = -99,
    MLC_CONNECTION_CONNECT_FAILED = -100,
    MLC_UNKNOW_CONNECTION_PKG_TYPE = -101,
    MLC_NULL_SESSION = -102,
    MLC_UNKNOW_CONNECTION_CTL_TYPE = -103,
    MLC_CLIENT_RECV_CONNECT = -104,
    MLC_SERVER_RECV_CONNECT_RSP = -105,
    MLC_HEARTBEAT_TIMEOUT = -106,
	MLC_CONNECT_TIMEOUT = -107,
    MLC_CLOSE_BY_REMOTE = -108,
    MLC_CLOSE_BEFORE = -109,
    MLC_SESSION_CLOSE = -110,
    MLC_SESSION_STATE_ERROR = -111,
    MLC_RECONNECT = -112,
    MLC_REATTACH = -113,
    MLC_RESET = -115,
    MLC_RECV_ERR = -117,
    MLC_DATA_TOO_LARGE = 118,
    MLC_CHAIN_ALLOC_FAILED = 119,
    MLC_SESSION_WRITE_BUFFER_FULL = -120,
    MLC_SESSION_WRITE_QUEUE_FULL = -121,
    MLC_DEBUG_CLOSE = -200,
    MLC_TUNNEL_TCP_DISCONNECT = -300,
    MLC_TUNNEL_SESSION_DISCONNECT = -301,
    MLC_SESSION_FULL = -500,
    MLC_CONNECTION_FULL = -501,
    MLC_URL_ERROR = -1000,
};

#endif