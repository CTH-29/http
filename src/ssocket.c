#include "ssocket.h"
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INVALID_FD -1
#define CHECK_NULL(p, ret) \
    do                     \
    {                      \
        if (p == NULL)     \
            return ret;    \
    } while (0)

#define CHECK_SOCKET(sock, ret) \
    do                          \
    {                           \
        if (sock < 3)           \
            return ret;         \
    } while (0)

#ifdef SSOCKET_DEBUG
#define _ssocket_debug(...)  \
    do                       \
    {                        \
        printf(__VA_ARGS__); \
        printf("\n");        \
    } while (0)
#else
#define _ssocket_debug(...) ((void)0)
#endif

int _socket_wait(int socket_fd, int dir, int timeout_ms);
bool _check_addr(const char *addr);
void _set_tcp_opts(int socket_fd);

#define ssocket_write_wait(fd, tm) _socket_wait(fd, 2, tm)
#define ssocket_read_wait(fd, tm) _socket_wait(fd, 1, tm)

/**
 * _socket_wait(): wait the socket ready to read or write.
 * @dir: 1 for read, 2 for write, 3 for both.
 * @timeout_ms: 1000ms = 1s
 * Return: 0 for ready, 1 for invilad params, 2 for timeout and 3 for connect errors.
 */
int _socket_wait(int socket_fd, int dir, int timeout_ms)
{
    int ret;
    fd_set rd_fds;
    fd_set wr_fds;
    struct timeval timeout;

    CHECK_SOCKET(socket_fd, -1);

    FD_ZERO(&rd_fds);
    FD_ZERO(&wr_fds);

    if (dir & 0x01)
        FD_SET(socket_fd, &rd_fds);
    if (dir & 0x02)
        FD_SET(socket_fd, &wr_fds);

    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;

    ret = select(socket_fd + 1, &rd_fds, &wr_fds, NULL, &timeout);

    if (ret <= -1)
        return 3;
    else if (ret == 0)
        return 2;
    else
        return 0;
}

bool _check_hostname(const char *hostname)
{
    int i, len = strlen(hostname);
    for (i = 0; i < len; i++)
        if (!((hostname[i] >= '0' && hostname[i] <= '9') || hostname[i] == '.'))
            return false;
    return true;
}

void _set_tcp_opts(int socket_fd)
{
    int tcp_nodelay = 1;
    int tcp_keepalive = 1;
    int tcp_keepcnt = 1;
    int tcp_keepidle = 45;
    int tcp_keepintvl = 30;
    struct timeval timeout = {3, 0};

    setsockopt(socket_fd, SOL_TCP, TCP_NODELAY, &tcp_nodelay, sizeof(tcp_nodelay));
    setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&tcp_keepalive, sizeof(tcp_keepalive));
    setsockopt(socket_fd, SOL_TCP, TCP_KEEPCNT, &tcp_keepcnt, sizeof(tcp_keepcnt));
    setsockopt(socket_fd, SOL_TCP, TCP_KEEPIDLE, &tcp_keepidle, sizeof(tcp_keepidle));
    setsockopt(socket_fd, SOL_TCP, TCP_KEEPINTVL, &tcp_keepintvl, sizeof(tcp_keepintvl));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    /* make socket_fd close on exec */
    fcntl(socket_fd, F_SETFD, FD_CLOEXEC);
}

ssocket_t *ssocket_create(int timeout_conn, int timeout_recv, int timeout_send)
{
    ssocket_t *sso = (ssocket_t *)malloc(sizeof(ssocket_t));
    if (sso == NULL)
        return NULL;
    sso->fd = INVALID_FD;
    sso->timeout_conn = timeout_conn > 0 ? timeout_conn : 0;
    sso->timeout_recv = timeout_recv > 0 ? timeout_recv : 0;
    sso->timeout_send = timeout_send > 0 ? timeout_send : 0;
    return sso;
}

void ssocket_destory(ssocket_t *sso)
{
    if (sso)
    {
        if (sso->fd != INVALID_FD)
            ssocket_disconnect(sso);
        if (sso->protocol)
            free(sso->protocol);
        if (sso->hostname)
            free(sso->hostname);
        free(sso);
    }
}

bool ssocket_set_addr(ssocket_t *sso, const char *protocol, const char *hostname, const char *port)
{
    CHECK_NULL(sso, false);
    if (protocol)
    {
        free(sso->protocol);
        sso->protocol = strdup(protocol);
    }
    if (hostname)
    {
        free(sso->hostname);
        sso->hostname = strdup(hostname);
    }
    if (port)
    {
        sso->port = atoi(port);
    }
}

/* protocol://hostname:port[/xxx] */
bool ssocket_set_url(ssocket_t *sso, const char *url)
{
    char *substr1, *substr2, *substr3, *portstr;
    CHECK_NULL(sso, false);

    free(sso->protocol);
    free(sso->hostname);

    substr1 = strstr(url, "://");
    if (substr1 == NULL)
        return false;
    sso->protocol = strndup(url, substr1 - url);

    substr1 += 3;
    substr2 = strstr(substr1, ":");
    substr3 = strstr(substr1, "/");
    if (substr3)
    {
        if (substr2 && substr2 < substr3)
        {
            sso->hostname = strndup(substr1, substr2 - substr1);
            substr2++;
            portstr = strndup(substr2, substr3 - substr2);
        }
        else
        {
            sso->hostname = strndup(substr1, substr3 - substr1);
        }
    }
    else
    {
        if (substr2)
        {
            sso->hostname = strndup(substr1, substr2 - substr1);
            substr2++;
            portstr = strdup(substr2);
        }
        else
        {
            sso->hostname = strdup(substr1);
        }
    }
    if (portstr)
    {
        sso->port = atoi(portstr);
        free(portstr);
    }
    else
    {
        sso->port = 0;
    }
    return true;
}

bool _socket_connect(int socket_fd, struct sockaddr *server, int timeout)
{
    int ret;
    int flags;
    if (socket_fd == INVALID_FD)
    {
        socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_fd == INVALID_FD)
        {
            _ssocket_debug("ssocket create socket failed");
            return false;
        }
        /* set socket to non-blocking mode */
        flags = fcntl(socket_fd, F_GETFL, 0);
        fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    }
    char *dat = (char *)server;
    for (size_t i = 0; i < sizeof(struct sockaddr); i++)
    {
        _ssocket_debug("%d ", dat[i]);
    }
    
    //_ssocket_debug("ssocket connecting %08x:%x", ntohl((((struct sockaddr_in*)server)->sin_addr), ntohs(((struct sockaddr_in*)server)->sin_port));
    ret = connect(socket_fd, server, sizeof(struct sockaddr));
    if (ret == 0)
        return true;
    else if (errno != EINPROGRESS)
        return false;

    /*
     * use select to check write event, if the socket is writable and no errors,
     * then connect is complete successfully!
     */
    if (ssocket_write_wait(socket_fd, timeout) > 0)
        return false;

    int error = 0;
    socklen_t length = sizeof(error);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
        return false;

    if (error != 0)
        return false;

    fcntl(socket_fd, F_SETFL, flags);
    return true;
}

bool ssocket_connect(ssocket_t *sso)
{
    int ret;
    struct sockaddr_in server;

    CHECK_NULL(sso, false);

    /*
    if (!(strstr(sso->protocol, "TCP") || strstr(sso->protocol, "tcp")))
    {
        _ssocket_debug("only support tcp connection");
        return false;
    }
    */

    ssocket_dump(sso);
    if (_check_hostname(sso->hostname)) /* only consist of number and dot */
    {
        in_addr_t _addr = inet_addr(sso->hostname); /* only consider ipv4 */
        if (_addr == INADDR_NONE)
        {
            _ssocket_debug("ssocket can't resolve the hostname [%s]", sso->hostname);
            return false;
        }
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = _addr;
        server.sin_port = htons(sso->port);

        if (_socket_connect(sso->fd, (struct sockaddr*)&server,sso->timeout_conn))
            goto OK;
        else
            goto FAIL;
    }
    else
    {
//         ssocket_dump(sso);
//         char _port[20];
//         sprintf(_port,"%d",sso->port);
// 
//         struct addrinfo *servr_info, hints;
//         if( getaddrinfo(sso->hostname, _port, &hints, &servr_info) )
//         {
//             printf("Failed to getaddrinfo for %s:%s, sleep\n", sso->hostname, _port);
//             sleep(6);
//         }
//         ssocket_dump(sso);
//         for ( ; servr_info ; servr_info = servr_info->ai_next)
//         {
//             if (_socket_connect(sso->fd, servr_info->ai_addr,sso->timeout_conn))
//             {
//                 goto OK;
//             }
//             else
//             {
//                 close(sso->fd);
//             }
//         }
//         goto FAIL;
    }

FAIL:
    _ssocket_debug("ssocket connect failed");
    ssocket_disconnect(sso);
    return false;
OK:
    _ssocket_debug("ssocket connect success");
    _set_tcp_opts(sso->fd);
    return true;
}

bool ssocket_disconnect(ssocket_t *sso)
{
    CHECK_NULL(sso, false);
    shutdown(sso->fd, SHUT_RDWR);
    close(sso->fd);
    sso->fd = INVALID_FD;
    return true;
}

bool ssocket_send(ssocket_t *sso, const char *sbuf, int sbuf_len)
{
    int send_offset = 0;
    CHECK_NULL(sso, false);
    CHECK_SOCKET(sso->fd, false);
    CHECK_NULL(sbuf, false);

    while (sbuf_len > 0)
    {
        int n;
        if (ssocket_write_wait(sso->fd, sso->timeout_send) > 0)
            return false;
        n = send(sso->fd, sbuf + send_offset, sbuf_len, MSG_NOSIGNAL);
        if (n < 0)
        {
            if (errno != EAGAIN || errno != EWOULDBLOCK)
                return false;
            n = 0;
        }
        send_offset += n;
        sbuf_len -= n;
    }
    return true;
}
int ssocket_recv(ssocket_t *sso, char *rbuf, int rbuf_len)
{
    int ret;
    CHECK_NULL(sso, 0);
    CHECK_SOCKET(sso->fd, 0);
    CHECK_NULL(rbuf, 0);

    if (ssocket_read_wait(sso->fd, sso->timeout_recv) == 0)
    {
        ret = recv(sso->fd, rbuf, rbuf_len, 0);
        if (ret > 0)
        {
            rbuf[ret] = 0;
            return ret;
        }
        else
            return 0; /* TCP socket has been disconnected. */
    }
    else
    {
        return 0;
    }
}

bool ssocket_recv_ready(ssocket_t *sso, int time_out)
{
    CHECK_NULL(sso, false);
    CHECK_SOCKET(sso->fd, false);
    time_out = time_out > 0 ? time_out : 0;
    return (_socket_wait(sso->fd, 1, time_out) == 0) ? true : false;
}

void ssocket_dump(ssocket_t *sso)
{
    printf("ssocket_t dump:\n");
    printf("\tfd = %d\n", sso->fd);
    printf("\tprotocol = %s\n", sso->protocol);
    printf("\thostname = %s\n", sso->hostname);
    printf("\tport = %d\n", sso->port);
    printf("\ttimeout = %d,%d,%d\n", sso->timeout_conn, sso->timeout_recv, sso->timeout_send);
}