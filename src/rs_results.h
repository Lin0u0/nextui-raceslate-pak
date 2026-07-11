#ifndef RS_RESULTS_H
#define RS_RESULTS_H
#include <stdbool.h>
#include <stddef.h>
#define RS_MAX_CLASSIFICATIONS 90
#define RS_MAX_CLASSIFICATION_ENTRIES 64
typedef enum { RS_RESULT_QUALIFYING, RS_RESULT_SPRINT, RS_RESULT_RACE } RsResultKind;
typedef struct { int position; int grid; int laps; double points; char driver_id[48]; char driver_code[8]; char driver_name[96]; char constructor_id[48]; char constructor_name[64]; char status[64]; char time[32]; char q1[24],q2[24],q3[24]; bool fastest_lap; } RsClassificationEntry;
typedef struct { int round; RsResultKind kind; size_t entry_count; RsClassificationEntry entries[RS_MAX_CLASSIFICATION_ENTRIES]; } RsClassification;
typedef struct { size_t count; RsClassification classifications[RS_MAX_CLASSIFICATIONS]; } RsResultsCatalog;
bool rs_results_decode(const char *json,RsResultKind kind,RsResultsCatalog *catalog);
const RsClassification *rs_results_find(const RsResultsCatalog *catalog,int round,RsResultKind kind);
#endif
