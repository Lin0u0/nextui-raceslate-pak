#include "rs_weather.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int64_t days(int year, unsigned month, unsigned day) {
    int era; unsigned yoe, doy, doe;
    year -= month <= 2; era = (year >= 0 ? year : year - 399) / 400;
    yoe = (unsigned)(year - era * 400); doy = (153u * (month + (month > 2 ? (unsigned)-3 : 9u)) + 2u) / 5u + day - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (int64_t)era * 146097 + doe - 719468;
}
static bool timestamp(const char *value, int64_t *out) {
    int y,m,d,h,n;
    if (!value || sscanf(value, "%d-%d-%dT%d:%d", &y,&m,&d,&h,&n) != 5) return false;
    *out = days(y,(unsigned)m,(unsigned)d)*86400 + h*3600+n*60; return true;
}
bool rs_weather_decode(const char *json, RsWeatherSnapshot *out) {
    cJSON *root; const cJSON *hourly,*times,*temps,*rain,*wind; int count,i;
    if (!json || !out) return false;
    memset(out,0,sizeof(*out));
    root=cJSON_Parse(json);
    if(!root)return false;
    hourly=cJSON_GetObjectItemCaseSensitive(root,"hourly");
    times=cJSON_GetObjectItemCaseSensitive(hourly,"time"); temps=cJSON_GetObjectItemCaseSensitive(hourly,"temperature_2m");
    rain=cJSON_GetObjectItemCaseSensitive(hourly,"precipitation_probability"); wind=cJSON_GetObjectItemCaseSensitive(hourly,"wind_speed_10m");
    count=cJSON_GetArraySize(times);
    if(!cJSON_IsArray(times)||!cJSON_IsArray(temps)||!cJSON_IsArray(rain)||!cJSON_IsArray(wind)||count<=0||count>RS_MAX_WEATHER_POINTS||
       cJSON_GetArraySize(temps)!=count||cJSON_GetArraySize(rain)!=count||cJSON_GetArraySize(wind)!=count){cJSON_Delete(root);return false;}
    for(i=0;i<count;i++){
        const cJSON *t=cJSON_GetArrayItem(times,i),*a=cJSON_GetArrayItem(temps,i),*b=cJSON_GetArrayItem(rain,i),*c=cJSON_GetArrayItem(wind,i);
        RsWeatherPoint *point;
        if(!cJSON_IsString(t)||!cJSON_IsNumber(a)||!cJSON_IsNumber(b)||!cJSON_IsNumber(c))continue;
        point=&out->points[out->count];
        if(!timestamp(t->valuestring,&point->time_utc))continue;
        point->temperature_c=a->valuedouble; point->rain_probability=b->valueint; point->wind_kmh=c->valuedouble;
        out->count++;
    }
    cJSON_Delete(root); return out->count>0;
}
const RsWeatherPoint *rs_weather_nearest(const RsWeatherSnapshot *weather,int64_t target){
    const RsWeatherPoint *best=NULL; int64_t distance=INT64_MAX; size_t i;
    if(!weather)return NULL;
    for(i=0;i<weather->count;i++){int64_t d=llabs(weather->points[i].time_utc-target);if(d<distance){distance=d;best=&weather->points[i];}}
    return distance<=5400?best:NULL;
}
