#include "http.h"

#include "ssocket.h"

static char http_sbuf[2048];
static char http_rbuf[2048];

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

int _parse_url(const char *url, char *host, char *file)
{
    char *ptr1, *ptr2;
    int len = 0;

    if ((ptr1 = strstr(url, "http://")) == NULL)
        return -1;
    ptr1 += strlen("http://");
    if ((ptr2 = strchr(ptr1, '/')) == NULL)
        return -1;

    strncpy(host, ptr1, ptr2 - ptr1);
    strcpy(file, ptr2 + 1);
    return 0;
}

char *http_request(const char *mothed, const char *url, char *version, char *headers[], const char *contents)
{
    char host[128];
    char file[128];
    int offfset;

    ssocket_t *sso = ssocket_create(2000, 1000, 1000);
    ssocket_set_url(sso, url);
    if (!ssocket_connect(sso))
    {
        printf("http connect failed\n");
        goto FAIL;
    }
    _parse_url(url, host, file);
    offfset = snprintf(http_sbuf, sizeof(http_sbuf), "%s /%s %s\r\n", mothed, file, version);
    offfset += snprintf(http_sbuf + offfset, sizeof(http_sbuf) - offfset, "HOST: %s\r\n", host);

    for (int i = 0; headers[i]; i++)
    {
        strcat(http_sbuf,headers[i]);
        strcat(http_sbuf, "\r\n");
    }

    strcat(http_sbuf, "\r\n");

    if (contents)
    {
        strcat(http_sbuf, contents);
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

    ssocket_recv(sso, http_rbuf, 2048);
    printf("http request finish\n");
    return http_rbuf;
FAIL:
    ssocket_destory(sso);
    return NULL;
}

char *http_get(const char *url, bool use_http_1_1, char *headers[], const char *contents)
{
    return http_request("GET", url, use_http_1_1 ? "HTTP/1.1" : "HTTP/1.0", headers, contents);
}

char *http_post(const char *url, bool use_http_1_1, char *headers[], const char *contents)
{
    return http_request("POST", url, use_http_1_1 ? "HTTP/1.1" : "HTTP/1.0", headers, contents);
}
