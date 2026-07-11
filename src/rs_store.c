#define _POSIX_C_SOURCE 200809L
#include "rs_store.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *rs_store_read(const char *path) {
    FILE *file = fopen(path, "rb");
    long length;
    char *bytes;
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || length > 8 * 1024 * 1024) { fclose(file); return NULL; }
    rewind(file);
    bytes = malloc((size_t)length + 1);
    if (!bytes || fread(bytes, 1, (size_t)length, file) != (size_t)length) { free(bytes); fclose(file); return NULL; }
    bytes[length] = '\0';
    fclose(file);
    return bytes;
}

bool rs_store_mkdirs(const char *path) {
    char copy[1024];
    char *cursor;
    size_t length;
    if (!path || (length = strlen(path)) == 0 || length >= sizeof(copy)) return false;
    memcpy(copy, path, length + 1);
    for (cursor = copy + 1; *cursor; cursor++) {
        if (*cursor == '/') { *cursor = '\0'; if (mkdir(copy, 0755) != 0 && errno != EEXIST) return false; *cursor = '/'; }
    }
    return mkdir(copy, 0755) == 0 || errno == EEXIST;
}

bool rs_store_write_atomic(const char *path, const char *bytes) {
    char temp[1100];
    char parent[1024];
    char *slash;
    int fd;
    size_t length, offset = 0;
    if (!path || !bytes || strlen(path) + 16 >= sizeof(temp) || strlen(path) >= sizeof(parent)) return false;
    snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '/');
    if (slash) { *slash = '\0'; if (!rs_store_mkdirs(parent)) return false; }
    snprintf(temp, sizeof(temp), "%s.tmp.XXXXXX", path);
    fd = mkstemp(temp);
    if (fd < 0) return false;
    length = strlen(bytes);
    while (offset < length) {
        ssize_t wrote = write(fd, bytes + offset, length - offset);
        if (wrote <= 0) { close(fd); unlink(temp); return false; }
        offset += (size_t)wrote;
    }
    if (fsync(fd) != 0 || close(fd) != 0 || rename(temp, path) != 0) { unlink(temp); return false; }
    return true;
}
