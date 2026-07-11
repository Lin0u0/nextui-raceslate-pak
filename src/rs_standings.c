#include "rs_standings.h"

#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

static const char *text(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static bool copy(char *dst, size_t size, const char *src) {
    size_t length;
    if (!src) return false;
    length = strlen(src);
    if (length >= size) return false;
    memcpy(dst, src, length + 1);
    return true;
}

static const cJSON *standings_list(cJSON *root, int *season, int *round) {
    const cJSON *mr = cJSON_GetObjectItemCaseSensitive(root, "MRData");
    const cJSON *table = cJSON_IsObject(mr) ? cJSON_GetObjectItemCaseSensitive(mr, "StandingsTable") : NULL;
    const cJSON *lists = cJSON_IsObject(table) ? cJSON_GetObjectItemCaseSensitive(table, "StandingsLists") : NULL;
    const cJSON *list = cJSON_IsArray(lists) ? cJSON_GetArrayItem(lists, 0) : NULL;
    if (!cJSON_IsObject(list)) return NULL;
    *season = atoi(text(list, "season"));
    *round = atoi(text(list, "round"));
    return list;
}

bool rs_standings_decode_drivers(const char *json, RsStandings *out) {
    cJSON *root;
    const cJSON *list, *array, *item;
    bool ok = false;
    if (!json || !out) return false;
    memset(out, 0, sizeof(*out));
    root = cJSON_Parse(json);
    if (!root) return false;
    list = standings_list(root, &out->season, &out->round);
    array = list ? cJSON_GetObjectItemCaseSensitive(list, "DriverStandings") : NULL;
    if (!cJSON_IsArray(array)) goto done;
    cJSON_ArrayForEach(item, array) {
        const cJSON *driver, *constructors, *constructor;
        RsDriverStanding *entry;
        if (out->driver_count >= RS_MAX_DRIVERS) goto done;
        entry = &out->drivers[out->driver_count];
        driver = cJSON_GetObjectItemCaseSensitive(item, "Driver");
        constructors = cJSON_GetObjectItemCaseSensitive(item, "Constructors");
        constructor = cJSON_IsArray(constructors) ? cJSON_GetArrayItem(constructors, 0) : NULL;
        if (!cJSON_IsObject(driver) ||
            !copy(entry->id, sizeof(entry->id), text(driver, "driverId")) ||
            !copy(entry->given_name, sizeof(entry->given_name), text(driver, "givenName")) ||
            !copy(entry->family_name, sizeof(entry->family_name), text(driver, "familyName")) ||
            !copy(entry->constructor_name, sizeof(entry->constructor_name), text(constructor, "name"))) goto done;
        copy(entry->code, sizeof(entry->code), text(driver, "code") ? text(driver, "code") : "---");
        entry->position = atoi(text(item, "position"));
        entry->points = strtod(text(item, "points"), NULL);
        entry->wins = atoi(text(item, "wins"));
        out->driver_count++;
    }
    ok = out->driver_count > 0;
done:
    cJSON_Delete(root);
    if (!ok) memset(out, 0, sizeof(*out));
    return ok;
}

bool rs_standings_decode_constructors(const char *json, RsStandings *out) {
    cJSON *root;
    const cJSON *list, *array, *item;
    bool ok = false;
    if (!json || !out) return false;
    root = cJSON_Parse(json);
    if (!root) return false;
    list = standings_list(root, &out->season, &out->round);
    array = list ? cJSON_GetObjectItemCaseSensitive(list, "ConstructorStandings") : NULL;
    if (!cJSON_IsArray(array)) goto done;
    cJSON_ArrayForEach(item, array) {
        const cJSON *constructor = cJSON_GetObjectItemCaseSensitive(item, "Constructor");
        RsConstructorStanding *entry;
        if (out->constructor_count >= RS_MAX_CONSTRUCTORS) goto done;
        entry = &out->constructors[out->constructor_count];
        if (!cJSON_IsObject(constructor) ||
            !copy(entry->id, sizeof(entry->id), text(constructor, "constructorId")) ||
            !copy(entry->name, sizeof(entry->name), text(constructor, "name"))) goto done;
        entry->position = atoi(text(item, "position"));
        entry->points = strtod(text(item, "points"), NULL);
        entry->wins = atoi(text(item, "wins"));
        out->constructor_count++;
    }
    ok = out->constructor_count > 0;
done:
    cJSON_Delete(root);
    if (!ok) memset(out, 0, sizeof(*out));
    return ok;
}
