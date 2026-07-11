#include "rs_http.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RS_HTTP_LIMIT (1024u * 1024u)

static size_t receive(void *data, size_t size, size_t count, void *context) {
    RsHttpResponse *response = context;
    size_t incoming = size * count;
    char *grown;
    if (incoming > RS_HTTP_LIMIT || response->length > RS_HTTP_LIMIT - incoming) return 0;
    grown = realloc(response->bytes, response->length + incoming + 1);
    if (!grown) return 0;
    response->bytes = grown;
    memcpy(response->bytes + response->length, data, incoming);
    response->length += incoming;
    response->bytes[response->length] = '\0';
    return incoming;
}

bool rs_http_get_https(const char *url, const char *ca_file, RsHttpResponse *out) {
    CURL *curl;
    CURLcode code;
    if (!url || strncmp(url, "https://", 8) != 0 || !out) return false;
    memset(out, 0, sizeof(*out));
    curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "RaceSlate/0.1 (+https://github.com/Lin0u0/RaceSlate)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    if (ca_file && *ca_file) curl_easy_setopt(curl, CURLOPT_CAINFO, ca_file);
    code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out->status);
    if (code != CURLE_OK) snprintf(out->error, sizeof(out->error), "%s", curl_easy_strerror(code));
    curl_easy_cleanup(curl);
    return code == CURLE_OK && out->status == 200 && out->bytes != NULL;
}

void rs_http_response_dispose(RsHttpResponse *response) {
    if (!response) return;
    free(response->bytes);
    memset(response, 0, sizeof(*response));
}
