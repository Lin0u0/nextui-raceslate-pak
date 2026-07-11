#ifndef RS_WEATHER_H
#define RS_WEATHER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RS_MAX_WEATHER_POINTS 400
typedef struct { int64_t time_utc; double temperature_c; int rain_probability; double wind_kmh; } RsWeatherPoint;
typedef struct { size_t count; RsWeatherPoint points[RS_MAX_WEATHER_POINTS]; } RsWeatherSnapshot;

bool rs_weather_decode(const char *json, RsWeatherSnapshot *out);
const RsWeatherPoint *rs_weather_nearest(const RsWeatherSnapshot *weather, int64_t time_utc);

#endif
