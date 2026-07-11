#include "rs_season.h"

#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int64_t days_from_civil(int year, unsigned month, unsigned day) {
    int era;
    unsigned year_of_era;
    unsigned day_of_year;
    unsigned day_of_era;
    year -= month <= 2;
    era = (year >= 0 ? year : year - 399) / 400;
    year_of_era = (unsigned)(year - era * 400);
    day_of_year = (153u * (month + (month > 2 ? (unsigned)-3 : 9u)) + 2u) / 5u + day - 1u;
    day_of_era = year_of_era * 365u + year_of_era / 4u - year_of_era / 100u + day_of_year;
    return (int64_t)era * 146097 + (int64_t)day_of_era - 719468;
}

static bool parse_utc(const char *date, const char *time, int64_t *out) {
    int year, month, day, hour, minute, second;
    if (!date || !time ||
        sscanf(date, "%d-%d-%d", &year, &month, &day) != 3 ||
        sscanf(time, "%d:%d:%dZ", &hour, &minute, &second) != 3 ||
        month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 60) {
        return false;
    }
    *out = days_from_civil(year, (unsigned)month, (unsigned)day) * 86400 +
           hour * 3600 + minute * 60 + second;
    return true;
}

static const char *string_value(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static bool copy_string(char *out, size_t capacity, const char *value) {
    size_t length;
    if (!value) return false;
    length = strlen(value);
    if (length >= capacity) return false;
    memcpy(out, value, length + 1);
    return true;
}

static bool append_session(RsEvent *event, const cJSON *race,
                           const char *field, RsSessionKind kind) {
    const cJSON *object = cJSON_GetObjectItemCaseSensitive(race, field);
    RsSession *session;
    if (!object) return true;
    if (!cJSON_IsObject(object) || event->session_count >= RS_MAX_SESSIONS) return false;
    session = &event->sessions[event->session_count];
    if (!parse_utc(string_value(object, "date"), string_value(object, "time"),
                   &session->starts_at_utc)) return false;
    session->kind = kind;
    event->session_count++;
    return true;
}

static int compare_sessions(const void *left, const void *right) {
    const RsSession *a = left;
    const RsSession *b = right;
    return (a->starts_at_utc > b->starts_at_utc) -
           (a->starts_at_utc < b->starts_at_utc);
}

bool rs_season_decode_schedule(const char *json, RsSeasonSnapshot *out) {
    cJSON *root = NULL;
    const cJSON *mrdata, *table, *races, *race;
    int index = 0;
    bool ok = false;
    if (!json || !out) return false;
    memset(out, 0, sizeof(*out));
    root = cJSON_Parse(json);
    if (!root) return false;
    mrdata = cJSON_GetObjectItemCaseSensitive(root, "MRData");
    table = cJSON_IsObject(mrdata) ? cJSON_GetObjectItemCaseSensitive(mrdata, "RaceTable") : NULL;
    races = cJSON_IsObject(table) ? cJSON_GetObjectItemCaseSensitive(table, "Races") : NULL;
    if (!cJSON_IsArray(races) || !string_value(table, "season")) goto done;
    out->season = atoi(string_value(table, "season"));
    cJSON_ArrayForEach(race, races) {
        const cJSON *circuit, *location;
        RsEvent *event;
        int64_t race_start;
        if (index >= RS_MAX_EVENTS || !cJSON_IsObject(race)) goto done;
        event = &out->events[index];
        circuit = cJSON_GetObjectItemCaseSensitive(race, "Circuit");
        location = cJSON_IsObject(circuit) ? cJSON_GetObjectItemCaseSensitive(circuit, "Location") : NULL;
        if (!copy_string(event->name, sizeof(event->name), string_value(race, "raceName")) ||
            !copy_string(event->circuit_id, sizeof(event->circuit_id), string_value(circuit, "circuitId")) ||
            !copy_string(event->circuit_name, sizeof(event->circuit_name), string_value(circuit, "circuitName")) ||
            !copy_string(event->locality, sizeof(event->locality), string_value(location, "locality")) ||
            !copy_string(event->country, sizeof(event->country), string_value(location, "country")) ||
            !parse_utc(string_value(race, "date"), string_value(race, "time"), &race_start)) goto done;
        event->round = atoi(string_value(race, "round"));
        event->latitude = strtod(string_value(location, "lat"), NULL);
        event->longitude = strtod(string_value(location, "long"), NULL);
        if (!append_session(event, race, "FirstPractice", RS_SESSION_PRACTICE_1) ||
            !append_session(event, race, "SecondPractice", RS_SESSION_PRACTICE_2) ||
            !append_session(event, race, "ThirdPractice", RS_SESSION_PRACTICE_3) ||
            !append_session(event, race, "SprintQualifying", RS_SESSION_SPRINT_QUALIFYING) ||
            !append_session(event, race, "Sprint", RS_SESSION_SPRINT) ||
            !append_session(event, race, "Qualifying", RS_SESSION_QUALIFYING) ||
            event->session_count >= RS_MAX_SESSIONS) goto done;
        event->sessions[event->session_count++] = (RsSession){RS_SESSION_RACE, race_start};
        event->is_sprint_weekend = cJSON_GetObjectItemCaseSensitive(race, "Sprint") != NULL;
        qsort(event->sessions, event->session_count, sizeof(event->sessions[0]), compare_sessions);
        index++;
    }
    out->event_count = (size_t)index;
    ok = out->season >= 1950 && out->event_count > 0;
done:
    cJSON_Delete(root);
    if (!ok) memset(out, 0, sizeof(*out));
    return ok;
}

const RsSession *rs_season_next_session(const RsSeasonSnapshot *snapshot,
                                        int64_t now_utc) {
    const RsSession *next = NULL;
    size_t event_index, session_index;
    if (!snapshot) return NULL;
    for (event_index = 0; event_index < snapshot->event_count; event_index++) {
        const RsEvent *event = &snapshot->events[event_index];
        for (session_index = 0; session_index < event->session_count; session_index++) {
            const RsSession *candidate = &event->sessions[session_index];
            if (candidate->starts_at_utc > now_utc &&
                (!next || candidate->starts_at_utc < next->starts_at_utc)) next = candidate;
        }
    }
    return next;
}
