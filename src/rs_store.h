#ifndef RS_STORE_H
#define RS_STORE_H

#include <stdbool.h>

char *rs_store_read(const char *path);
bool rs_store_write_atomic(const char *path, const char *bytes);
bool rs_store_mkdirs(const char *path);

#endif
