#include "rs_profiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *destination, size_t capacity, const char *source) {
    if (capacity == 0) return;
    snprintf(destination, capacity, "%s", source ? source : "");
}

static size_t split_tabs(char *line, char **fields, size_t capacity) {
    size_t count = 0;
    char *cursor = line;
    while (count < capacity) {
        char *tab;
        fields[count++] = cursor;
        tab = strchr(cursor, '\t');
        if (!tab) break;
        *tab = '\0';
        cursor = tab + 1;
    }
    if (count > 0) fields[count - 1][strcspn(fields[count - 1], "\r\n")] = '\0';
    return count;
}

static void decode_series(char *text, RsProfile *profile) {
    char *item = text;
    while (item && *item && profile->series_count < RS_MAX_PROFILE_SERIES) {
        char *next = strchr(item, ',');
        RsProfilePoint point;
        if (next) *next = '\0';
        if (sscanf(item, "%d:%d:%lf", &point.round, &point.position, &point.points) == 3) {
            profile->series[profile->series_count++] = point;
        }
        item = next ? next + 1 : NULL;
    }
}

bool rs_profiles_load(const char *path, RsProfileCatalog *catalog) {
    FILE *file;
    char line[4096];
    if (!path || !catalog) return false;
    memset(catalog, 0, sizeof(*catalog));
    file = fopen(path, "rb");
    if (!file) return false;
    if (!fgets(line, sizeof(line), file)) { fclose(file); return false; }
    while (catalog->count < RS_MAX_PROFILES && fgets(line, sizeof(line), file)) {
        char *fields[11];
        RsProfile *profile;
        if (split_tabs(line, fields, 11) != 11) continue;
        profile = &catalog->profiles[catalog->count++];
        profile->type = fields[0][0] == 'C' ? RS_PROFILE_CONSTRUCTOR : RS_PROFILE_DRIVER;
        copy_text(profile->provider_id, sizeof(profile->provider_id), fields[1]);
        copy_text(profile->name, sizeof(profile->name), fields[3]);
        copy_text(profile->country, sizeof(profile->country), fields[4]);
        profile->starts = atoi(fields[5]); profile->wins = atoi(fields[6]);
        profile->podiums = atoi(fields[7]); profile->poles = atoi(fields[8]);
        profile->championships = atoi(fields[9]);
        decode_series(fields[10], profile);
    }
    fclose(file);
    return catalog->count > 0;
}

const RsProfile *rs_profiles_find(const RsProfileCatalog *catalog, RsProfileType type, const char *provider_id) {
    size_t index;
    if (!catalog || !provider_id) return NULL;
    for (index = 0; index < catalog->count; index++) {
        if (catalog->profiles[index].type == type && strcmp(catalog->profiles[index].provider_id, provider_id) == 0) return &catalog->profiles[index];
    }
    return NULL;
}

void rs_profiles_rebuild_series(RsProfileCatalog *catalog, const RsResultsCatalog *results) {
    double totals[RS_MAX_PROFILES] = {0};
    int round, maximum_round = 0;
    size_t index;
    if (!catalog || !results) return;
    for (index = 0; index < catalog->count; index++) catalog->profiles[index].series_count = 0;
    for (index = 0; index < results->count; index++) if (results->classifications[index].round > maximum_round) maximum_round = results->classifications[index].round;
    for (round = 1; round <= maximum_round && round <= RS_MAX_PROFILE_SERIES; round++) {
        size_t classification_index;
        for (classification_index = 0; classification_index < results->count; classification_index++) {
            const RsClassification *classification = &results->classifications[classification_index];
            size_t entry_index;
            if (classification->round != round || (classification->kind != RS_RESULT_RACE && classification->kind != RS_RESULT_SPRINT)) continue;
            for (entry_index = 0; entry_index < classification->entry_count; entry_index++) {
                const RsClassificationEntry *entry = &classification->entries[entry_index];
                for (index = 0; index < catalog->count; index++) {
                    RsProfile *profile = &catalog->profiles[index];
                    const char *id = profile->type == RS_PROFILE_DRIVER ? entry->driver_id : entry->constructor_id;
                    if (!strcmp(profile->provider_id, id)) totals[index] += entry->points;
                }
            }
        }
        for (index = 0; index < catalog->count; index++) {
            RsProfile *profile = &catalog->profiles[index];
            size_t other; int position = 1;
            for (other = 0; other < catalog->count; other++) if (catalog->profiles[other].type == profile->type && totals[other] > totals[index]) position++;
            if (profile->series_count < RS_MAX_PROFILE_SERIES) profile->series[profile->series_count++] = (RsProfilePoint){round, position, totals[index]};
        }
    }
}
