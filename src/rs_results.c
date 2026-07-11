#include "rs_results.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *text_value(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static int integer_value(const cJSON *object, const char *key) {
    const char *value = text_value(object, key);
    return value ? atoi(value) : 0;
}

static void copy_value(char *destination, size_t capacity, const char *source) {
    snprintf(destination, capacity, "%s", source ? source : "");
}

const RsClassification *rs_results_find(const RsResultsCatalog *catalog, int round, RsResultKind kind) {
    size_t index;
    if (!catalog) return NULL;
    for (index = 0; index < catalog->count; index++) {
        if (catalog->classifications[index].round == round && catalog->classifications[index].kind == kind) return &catalog->classifications[index];
    }
    return NULL;
}

static RsClassification *classification_slot(RsResultsCatalog *catalog, int round, RsResultKind kind) {
    size_t index;
    for (index = 0; index < catalog->count; index++) {
        if (catalog->classifications[index].round == round && catalog->classifications[index].kind == kind) return &catalog->classifications[index];
    }
    if (catalog->count >= RS_MAX_CLASSIFICATIONS) return NULL;
    catalog->classifications[catalog->count] = (RsClassification){.round = round, .kind = kind};
    return &catalog->classifications[catalog->count++];
}

bool rs_results_decode(const char *json, RsResultKind kind, RsResultsCatalog *catalog) {
    cJSON *root;
    const cJSON *mr_data, *table, *races, *race;
    const char *key = kind == RS_RESULT_RACE ? "Results" : kind == RS_RESULT_SPRINT ? "SprintResults" : "QualifyingResults";
    if (!json || !catalog) return false;
    root = cJSON_Parse(json);
    if (!root) return false;
    mr_data = cJSON_GetObjectItemCaseSensitive(root, "MRData");
    table = cJSON_GetObjectItemCaseSensitive(mr_data, "RaceTable");
    races = cJSON_GetObjectItemCaseSensitive(table, "Races");
    if (!cJSON_IsArray(races)) { cJSON_Delete(root); return false; }
    cJSON_ArrayForEach(race, races) {
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(race, key), *row;
        RsClassification *classification;
        if (!cJSON_IsArray(rows)) continue;
        classification = classification_slot(catalog, integer_value(race, "round"), kind);
        if (!classification) { cJSON_Delete(root); return false; }
        classification->entry_count = 0;
        cJSON_ArrayForEach(row, rows) {
            const cJSON *driver, *constructor, *fastest_lap, *time;
            RsClassificationEntry *entry;
            char name[96];
            if (classification->entry_count >= RS_MAX_CLASSIFICATION_ENTRIES) { cJSON_Delete(root); return false; }
            entry = &classification->entries[classification->entry_count++];
            memset(entry, 0, sizeof(*entry));
            driver = cJSON_GetObjectItemCaseSensitive(row, "Driver");
            constructor = cJSON_GetObjectItemCaseSensitive(row, "Constructor");
            entry->position = integer_value(row, "position"); entry->grid = integer_value(row, "grid");
            entry->laps = integer_value(row, "laps"); entry->points = strtod(text_value(row, "points") ? text_value(row, "points") : "0", NULL);
            copy_value(entry->driver_id, sizeof(entry->driver_id), text_value(driver, "driverId"));
            copy_value(entry->driver_code, sizeof(entry->driver_code), text_value(driver, "code"));
            snprintf(name, sizeof(name), "%s %s", text_value(driver, "givenName") ? text_value(driver, "givenName") : "", text_value(driver, "familyName") ? text_value(driver, "familyName") : "");
            copy_value(entry->driver_name, sizeof(entry->driver_name), name);
            copy_value(entry->constructor_id, sizeof(entry->constructor_id), text_value(constructor, "constructorId"));
            copy_value(entry->constructor_name, sizeof(entry->constructor_name), text_value(constructor, "name"));
            copy_value(entry->status, sizeof(entry->status), text_value(row, "status") ? text_value(row, "status") : "CLASSIFIED");
            copy_value(entry->q1, sizeof(entry->q1), text_value(row, "Q1")); copy_value(entry->q2, sizeof(entry->q2), text_value(row, "Q2")); copy_value(entry->q3, sizeof(entry->q3), text_value(row, "Q3"));
            time = cJSON_GetObjectItemCaseSensitive(row, "Time"); copy_value(entry->time, sizeof(entry->time), text_value(time, "time"));
            fastest_lap = cJSON_GetObjectItemCaseSensitive(row, "FastestLap"); entry->fastest_lap = cJSON_IsObject(fastest_lap) && integer_value(fastest_lap, "rank") == 1;
        }
    }
    cJSON_Delete(root);
    return true;
}
