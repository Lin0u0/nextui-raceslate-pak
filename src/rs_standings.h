#ifndef RS_STANDINGS_H
#define RS_STANDINGS_H

#include <stdbool.h>
#include <stddef.h>

#define RS_MAX_DRIVERS 30
#define RS_MAX_CONSTRUCTORS 15

typedef struct {
    int position;
    double points;
    int wins;
    char id[48];
    char code[8];
    char given_name[48];
    char family_name[48];
    char constructor_name[64];
} RsDriverStanding;

typedef struct {
    int position;
    double points;
    int wins;
    char id[48];
    char name[64];
} RsConstructorStanding;

typedef struct {
    int season;
    int round;
    size_t driver_count;
    size_t constructor_count;
    RsDriverStanding drivers[RS_MAX_DRIVERS];
    RsConstructorStanding constructors[RS_MAX_CONSTRUCTORS];
} RsStandings;

bool rs_standings_decode_drivers(const char *json, RsStandings *out);
bool rs_standings_decode_constructors(const char *json, RsStandings *out);

#endif
