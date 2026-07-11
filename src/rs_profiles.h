#ifndef RS_PROFILES_H
#define RS_PROFILES_H

#include <stdbool.h>
#include <stddef.h>
#include "rs_results.h"
#include "rs_standings.h"

#define RS_MAX_PROFILES (RS_MAX_DRIVERS + RS_MAX_CONSTRUCTORS)
#define RS_MAX_PROFILE_SERIES 32

typedef enum { RS_PROFILE_DRIVER, RS_PROFILE_CONSTRUCTOR } RsProfileType;

typedef struct {
    int round;
    int position;
    double points;
} RsProfilePoint;

typedef struct {
    RsProfileType type;
    char provider_id[40];
    char name[64];
    char country[40];
    int starts;
    int wins;
    int podiums;
    int poles;
    int championships;
    bool season_only;
    RsProfilePoint series[RS_MAX_PROFILE_SERIES];
    size_t series_count;
} RsProfile;

typedef struct {
    RsProfile profiles[RS_MAX_PROFILES];
    size_t count;
} RsProfileCatalog;

bool rs_profiles_load(const char *path, RsProfileCatalog *catalog);
const RsProfile *rs_profiles_find(const RsProfileCatalog *catalog, RsProfileType type, const char *provider_id);
void rs_profiles_rebuild_series(RsProfileCatalog *catalog, const RsResultsCatalog *results);
void rs_profiles_build_season(RsProfileCatalog *catalog,const RsStandings *standings,const RsResultsCatalog *results);
bool rs_profiles_apply_career(const char *path,RsProfileCatalog *catalog);
bool rs_profiles_decode_progression(const char *json,RsProfileCatalog *catalog);

#endif
