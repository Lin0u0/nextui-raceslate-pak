#ifndef RS_SETTINGS_H
#define RS_SETTINGS_H

typedef enum { RS_TEXT_COMPACT, RS_TEXT_STANDARD, RS_TEXT_LARGE, RS_TEXT_SIZE_COUNT } RsTextSize;

RsTextSize rs_text_size_cycle(RsTextSize value);
RsTextSize rs_text_size_decode(const char *text, RsTextSize fallback);

#endif
