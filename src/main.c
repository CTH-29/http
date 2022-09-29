#include "http.h"

char *header_lines[] =
    {
        "Connection: keep-alive"};

int main(int argc, char *argv[])
{
    char *version;
    char *status_msg;
    char *headers;
    char *content;

    char *recv_data = http_get("http://mes.bitmain.com:51611/api/Default/GetMes", true, header_lines, NULL);
    if (recv_data)
    {
        int status_code = http_parse_response(recv_data, &version, &status_msg, &headers, &content);
        printf("\nstatus_code = %d\n", status_code);
        printf("version = %s\n", version);
        printf("status_msg = %s\n", status_msg);
        printf("headers = \n%s\n", headers);
        printf("content = \n%s\n", content);
    }
    return 0;
}
