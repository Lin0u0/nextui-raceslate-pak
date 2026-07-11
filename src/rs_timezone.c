#define _POSIX_C_SOURCE 200809L
#include "rs_timezone.h"
#include <string.h>
#include <time.h>

static int month_at(int64_t utc){time_t raw=(time_t)utc;struct tm value;gmtime_r(&raw,&value);return value.tm_mon+1;}
int rs_track_utc_offset(const char *id,int64_t utc){int month=month_at(utc);int europe_dst=month>=4&&month<=10;int us_dst=month>=4&&month<=10;if(!id)return 0;
 if(!strcmp(id,"albert_park"))return (month<=3||month>=10)?11*3600:10*3600;
 if(!strcmp(id,"shanghai")||!strcmp(id,"marina_bay"))return 8*3600;if(!strcmp(id,"suzuka"))return 9*3600;
 if(!strcmp(id,"miami")||!strcmp(id,"villeneuve"))return (us_dst?-4:-5)*3600;if(!strcmp(id,"americas"))return (us_dst?-5:-6)*3600;if(!strcmp(id,"vegas"))return (us_dst?-7:-8)*3600;
 if(!strcmp(id,"silverstone"))return europe_dst?3600:0;if(!strcmp(id,"catalunya")||!strcmp(id,"monaco")||!strcmp(id,"red_bull_ring")||!strcmp(id,"spa")||!strcmp(id,"hungaroring")||!strcmp(id,"zandvoort")||!strcmp(id,"monza")||!strcmp(id,"madring"))return europe_dst?7200:3600;
 if(!strcmp(id,"baku")||!strcmp(id,"yas_marina"))return 4*3600;if(!strcmp(id,"losail"))return 3*3600;if(!strcmp(id,"rodriguez"))return -6*3600;if(!strcmp(id,"interlagos"))return -3*3600;return 0;}
