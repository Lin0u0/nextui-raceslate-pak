#ifndef RS_REFERENCE_H
#define RS_REFERENCE_H
#include <stdbool.h>
#include <stddef.h>
#define RS_MAX_REFERENCES 30
typedef struct { char provider_id[48]; double length_km; int turns; char direction[24]; int first_year; int races; char lap_record[24]; char record_driver[64]; int record_year; char recent_winners[384]; char recent_poles[384]; char most_wins[96]; char most_poles[96]; } RsCircuitReference;
typedef struct { size_t count; RsCircuitReference circuits[RS_MAX_REFERENCES]; } RsReferenceCatalog;
bool rs_reference_load(const char *path, RsReferenceCatalog *out);
const RsCircuitReference *rs_reference_circuit(const RsReferenceCatalog *catalog, const char *provider_id);
#endif
