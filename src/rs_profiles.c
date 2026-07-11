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
