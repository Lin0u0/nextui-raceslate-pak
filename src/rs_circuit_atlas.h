#ifndef RS_CIRCUIT_ATLAS_H
#define RS_CIRCUIT_ATLAS_H
#include <stdbool.h>
#include <stddef.h>
#define RS_MAX_ATLAS_CIRCUITS 2000
typedef struct{int year;int round;char asset_id[64];double latitude;double longitude;}RsCircuitAtlasEntry;
typedef struct{size_t count;RsCircuitAtlasEntry entries[RS_MAX_ATLAS_CIRCUITS];}RsCircuitAtlas;
bool rs_circuit_atlas_load(const char *path,RsCircuitAtlas *atlas);
const char *rs_circuit_atlas_nearest(const RsCircuitAtlas *atlas,int year,int round,double latitude,double longitude);
#endif
