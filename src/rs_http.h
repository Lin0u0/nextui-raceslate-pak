#ifndef RS_HTTP_H
#define RS_HTTP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct { char *bytes; size_t length; long status; char error[160]; } RsHttpResponse;

bool rs_http_get_https(const char *url, const char *ca_file, RsHttpResponse *out);
void rs_http_response_dispose(RsHttpResponse *response);

#endif
