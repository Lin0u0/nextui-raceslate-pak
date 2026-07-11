#define _GNU_SOURCE
#include "rs_timezone.h"

#include <string.h>
#include <time.h>

static struct tm utc_parts(int64_t utc) {
    time_t raw = (time_t)utc;
    struct tm value;
    gmtime_r(&raw, &value);
    return value;
}

static int days_in_month(int year,int month){static const int values[]={31,28,31,30,31,30,31,31,30,31,30,31};if(month==2&&((year%4==0&&year%100!=0)||year%400==0))return 29;return values[month-1];}
static int sunday_in_month(int year,int month,int occurrence){struct tm first={0};time_t raw;first.tm_year=year-1900;first.tm_mon=month-1;first.tm_mday=1;raw=timegm(&first);first=utc_parts((int64_t)raw);return 1+(7-first.tm_wday)%7+(occurrence-1)*7;}
static int last_sunday(int year,int month){struct tm last={0};time_t raw;last.tm_year=year-1900;last.tm_mon=month-1;last.tm_mday=days_in_month(year,month);raw=timegm(&last);last=utc_parts((int64_t)raw);return days_in_month(year,month)-last.tm_wday;}
static int european_dst(struct tm value){int month=value.tm_mon+1,year=value.tm_year+1900;if(month<3||month>10)return 0;if(month>3&&month<10)return 1;if(month==3){int day=last_sunday(year,3);return value.tm_mday>day||(value.tm_mday==day&&value.tm_hour>=1);}else{int day=last_sunday(year,10);return value.tm_mday<day||(value.tm_mday==day&&value.tm_hour<1);}}
static int american_dst(struct tm value,int standard_offset){int month=value.tm_mon+1,year=value.tm_year+1900;if(month<3||month>11)return 0;if(month>3&&month<11)return 1;if(month==3){int day=sunday_in_month(year,3,2),hour=2-standard_offset;return value.tm_mday>day||(value.tm_mday==day&&value.tm_hour>=hour);}else{int day=sunday_in_month(year,11,1),hour=1-standard_offset;return value.tm_mday<day||(value.tm_mday==day&&value.tm_hour<hour);}}
static int melbourne_dst(struct tm value){int month=value.tm_mon+1,year=value.tm_year+1900;if(month>4&&month<10)return 0;if(month<4||month>10)return 1;if(month==4)return value.tm_mday<sunday_in_month(year,4,1);return value.tm_mday>=sunday_in_month(year,10,1);
}

int rs_track_utc_offset(const char *id, int64_t utc) {
    struct tm value=utc_parts(utc);
    int europe_dst=european_dst(value);
    if (!id) return 0;
    if (!strcmp(id, "albert_park")) return (melbourne_dst(value) ? 11 : 10) * 3600;
    if (!strcmp(id, "shanghai") || !strcmp(id, "marina_bay")) return 8 * 3600;
    if (!strcmp(id, "suzuka")) return 9 * 3600;
    if (!strcmp(id, "miami") || !strcmp(id, "villeneuve")) return (american_dst(value,-5) ? -4 : -5) * 3600;
    if (!strcmp(id, "americas")) return (american_dst(value,-6) ? -5 : -6) * 3600;
    if (!strcmp(id, "vegas")) return (american_dst(value,-8) ? -7 : -8) * 3600;
    if (!strcmp(id, "silverstone")) return europe_dst ? 3600 : 0;
    if (!strcmp(id, "catalunya") || !strcmp(id, "monaco") || !strcmp(id, "red_bull_ring") ||
        !strcmp(id, "spa") || !strcmp(id, "hungaroring") || !strcmp(id, "zandvoort") ||
        !strcmp(id, "monza") || !strcmp(id, "madring")) return europe_dst ? 7200 : 3600;
    if (!strcmp(id, "baku") || !strcmp(id, "yas_marina")) return 4 * 3600;
    if (!strcmp(id, "losail")) return 3 * 3600;
    if (!strcmp(id, "rodriguez")) return -6 * 3600;
    if (!strcmp(id, "interlagos")) return -3 * 3600;
    return 0;
}
