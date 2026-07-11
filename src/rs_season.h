#ifndef RS_SEASON_H
#define RS_SEASON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RS_MAX_EVENTS 30
#define RS_MAX_SESSIONS 6

typedef enum {
    RS_SESSION_PRACTICE_1,
    RS_SESSION_PRACTICE_2,
    RS_SESSION_PRACTICE_3,
    RS_SESSION_SPRINT_QUALIFYING,
    RS_SESSION_SPRINT,
    RS_SESSION_QUALIFYING,
    RS_SESSION_RACE
} RsSessionKind;

typedef struct {
    RsSessionKind kind;
    int64_t starts_at_utc;
} RsSession;

typedef struct {
    int round;
    char name[96];
    char circuit_id[48];
    char circuit_name[96];
    char locality[48];
    char country[48];
    double latitude;
    double longitude;
    bool is_sprint_weekend;
    bool time_estimated;
    size_t session_count;
    RsSession sessions[RS_MAX_SESSIONS];
} RsEvent;

typedef struct {
    int season;
    size_t event_count;
    RsEvent events[RS_MAX_EVENTS];
} RsSeasonSnapshot;

bool rs_season_decode_schedule(const char *json, RsSeasonSnapshot *out);
const RsSession *rs_season_next_session(const RsSeasonSnapshot *snapshot,
                                        int64_t now_utc);

#endif
