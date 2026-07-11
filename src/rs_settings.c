#include "rs_settings.h"

#include <stdlib.h>
#include <string.h>

RsTextSize rs_text_size_cycle(RsTextSize value){return value>=RS_TEXT_COMPACT&&value<RS_TEXT_LARGE?(RsTextSize)(value+1):RS_TEXT_COMPACT;}

RsTextSize rs_text_size_decode(const char *text,RsTextSize fallback){const char *value;if(!text)return fallback;value=strstr(text,"text_size=");if(value){int parsed=atoi(value+10);if(parsed>=RS_TEXT_COMPACT&&parsed<RS_TEXT_SIZE_COUNT)return (RsTextSize)parsed;}return fallback;}
