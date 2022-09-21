#include "http.h"


char *header_lines[] =
    {
        "something 1",
        "something 2"};

int main(int argc, char *argv[])
{
    char *version;
    char *status_msg;
    char *headers;
    char *contents;

    char *recv_data = http_get("https://www.baidu.com/",true,NULL,NULL);
    if(recv_data)
    {
        int status_code = http_parse_response(recv_data,&version,&status_msg,&headers,&contents);
        printf("\nstatus_code = [%d]\n",status_code);
        printf("version = [%s]\n",version);
        printf("status_msg = [%s]\n",status_msg);
        printf("headers = [%s]\n",headers);
        printf("contents = [%s]\n",contents);
    }
    return 0;
}

