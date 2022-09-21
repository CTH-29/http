#ifndef __HTTP_H__
#define __HTTP_H__

#include <stdio.h>
#include <stdbool.h>

int http_parse_response(const char *resp, char **version, char **status_msg, char **headers, char **content);
char *http_request(const char *mothed, const char *url, char *version, char *header[], const char *content);

char *http_get(const char *url, bool use_http_1_1, char *headers[], const char *content);
char *http_post(const char *url, bool use_http_1_1, char *headers[], const char *content);

#endif
