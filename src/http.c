#include "http.h"

#include "ssocket.h"

static char http_sbuf[16*1024];
static char http_rbuf[32*1024];

char *_skip_whitespace(char *msg)
{
    switch (*msg)
    {
    case '\r':
    case '\n':
    case '\t':
    case ' ':
        msg++;
        break;
    default:
        return msg;
    }
    return msg;
}

int http_parse_response(const char *resp, char **version, char **status_msg, char **headers, char **contents)
{
    char *ptr1, *ptr2;
    char *str_tmp;
    int status_code;

    if (resp == NULL)
        goto FAIL;

    if ((ptr1 = strstr(resp, "HTTP/")) == NULL)
        goto FAIL;
    if ((ptr2 = strchr(ptr1, ' ')) == NULL)
        goto FAIL;
    *version = strndup(ptr1, ptr2 - ptr1);

    ptr1 = _skip_whitespace(ptr2);
    if ((ptr2 = strchr(ptr1, ' ')) == NULL)
        goto FAIL;
    str_tmp = strndup(ptr1, ptr2 - ptr1);
    status_code = atoi(str_tmp);
    free(str_tmp);

    ptr1 = ptr2 = _skip_whitespace(ptr2);
    if ((ptr2 = strstr(ptr1, "\r\n")) == NULL)
        goto FAIL;
    *status_msg = strndup(ptr1, ptr2 - ptr1);

    if ((ptr1 = ptr2 + 2) == NULL)
        goto FINE;
    if ((ptr2 = strstr(ptr1, "\r\n\r\n")) == NULL)
        goto FAIL;
    /* let the empty line to become the end of string('\0') */
    ptr2[2] = '\0';
    ptr2[3] = '\0';
    *headers = ptr1;
    *contents = ptr2 + 4;

    goto FINE;
FAIL:
    printf("http parse response err\n");
    return -1;
FINE:
    return status_code;
}

bool _parse_url(const char *url, char *host, char *port, char *uri)
{
    char *ptr1, *ptr2, *ptr3, *ptr4;
    int host_len = 0;

    ptr1 = strstr(url, "http://");
    if (ptr1 == NULL)
        return false;
    ptr1 += strlen("http://");

    if ((ptr3 = strchr(ptr1, '/')) == NULL)
    {
        uri[0] = 0;
        host_len = strlen(ptr1);
    }
    else
    {
        strcpy(uri, ptr3 + 1);
        host_len = ptr3 - ptr1;
    }

    ptr2 = strchr(ptr1, ':');
    if (ptr2 && ptr2 < ptr3)
    {
        host_len = ptr2 - ptr1;
        strncpy(port, ptr2 + 1, ptr3 - ptr2 - 1);
    }
    else
    {
        strcpy(port, "80");
    }

    strncpy(host, ptr1, host_len);
    return true;
}

char *http_request(const char *mothed, const char *url, char *version, char *headers[], const char *content)
{
    char host[128];
    char port[8];
    char uri[256];
    int offfset;

    ssocket_t *sso = ssocket_create(2000, 1000, 1000);

    if (_parse_url(url, host, port, uri) == false)
    {
        printf("http url error\n");
        goto FAIL;
    }

    if (!ssocket_connect_hostname(sso, host, port))
    {
        printf("http connect failed\n");
        goto FAIL;
    }

    offfset = snprintf(http_sbuf, sizeof(http_sbuf), "%s /%s %s\r\n", mothed, uri, version);
    offfset += snprintf(http_sbuf + offfset, sizeof(http_sbuf) - offfset, "Host: %s:%s\r\nAccept: */*\r\n", host, port);

    for (int i = 0; headers[i]; i++)
    {
        strcat(http_sbuf, headers[i]);
        strcat(http_sbuf, "\r\n");
    }

    strcat(http_sbuf, "\r\n");

    if (content)
    {
        strcat(http_sbuf, content);
        strcat(http_sbuf, "\r\n");
    }

    if (!ssocket_send_str(sso, http_sbuf))
    {
        printf("http send failed\n");
        goto FAIL;
    }

    if (!ssocket_recv_ready(sso, 5000))
    {
        printf("http recv failed\n");
        goto FAIL;
    }

    ssocket_recv(sso, http_rbuf, sizeof(http_rbuf));
    printf("http request finish\n");
    return http_rbuf;
FAIL:
    ssocket_destory(sso);
    return NULL;
}

char *http_get(const char *url, bool use_http_1_1, char *headers[], const char *content)
{
    return http_request("GET", url, use_http_1_1 ? "HTTP/1.1" : "HTTP/1.0", headers, content);
}

char *http_post(const char *url, bool use_http_1_1, char *headers[], const char *content)
{
    return http_request("POST", url, use_http_1_1 ? "HTTP/1.1" : "HTTP/1.0", headers, content);
}
