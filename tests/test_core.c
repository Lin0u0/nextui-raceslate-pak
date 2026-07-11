#include "rs_season.h"
#include "rs_app.h"
#include "rs_standings.h"
#include "rs_weather.h"
#include "rs_store.h"
#include "rs_results.h"
#include "rs_profiles.h"
#include "rs_timezone.h"
#include "rs_circuit_atlas.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    long size;
    char *bytes;
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size >= 0);
    rewind(file);
    bytes = malloc((size_t)size + 1);
    assert(bytes);
    assert(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    bytes[size] = '\0';
    fclose(file);
    return bytes;
}

static void user_sees_the_next_session_from_a_jolpica_schedule(const char *fixtures) {
    char path[512];
    char *json;
    RsSeasonSnapshot snapshot;
    const RsSession *next;

    snprintf(path, sizeof(path), "%s/jolpica_schedule.json", fixtures);
    json = read_file(path);
    assert(rs_season_decode_schedule(json, &snapshot));
    assert(snapshot.season == 2026);
    assert(snapshot.event_count == 2);

    next = rs_season_next_session(&snapshot, 1775797200); /* 2026-04-10 05:00Z */
    assert(next);
    assert(next->kind == RS_SESSION_PRACTICE_1);
    assert(next->starts_at_utc == 1775804400);
    assert(snapshot.events[1].round == 2);
    assert(snapshot.events[1].is_sprint_weekend);
    assert(snapshot.events[1].session_count == 5);
    assert(snapshot.events[1].sessions[1].kind == RS_SESSION_SPRINT_QUALIFYING);
    free(json);
}

static void brick_controls_navigate_the_public_app_state(void) {
    RsApp *app = rs_app_create();
    assert(app);
    assert(rs_app_route(app) == RS_ROUTE_NEXT);
    rs_app_dispatch(app,RS_ACTION_X);assert(rs_app_track_time(app));rs_app_dispatch(app,RS_ACTION_X);assert(!rs_app_track_time(app));
    rs_app_dispatch(app, RS_ACTION_R1);
    assert(rs_app_route(app) == RS_ROUTE_CALENDAR);
    rs_app_dispatch(app, RS_ACTION_L2);
    assert(rs_app_take_season_delta(app) == -1);
    assert(rs_app_take_season_delta(app) == 0);
    rs_app_dispatch(app, RS_ACTION_R2);
    assert(rs_app_take_season_delta(app) == 1);
    rs_app_set_cursor(app,RS_ROUTE_CALENDAR,7);assert(rs_app_cursor(app)==7);rs_app_set_cursor(app,RS_ROUTE_CALENDAR,0);
    rs_app_dispatch(app, RS_ACTION_R1);
    assert(rs_app_route(app) == RS_ROUTE_STANDINGS);
    rs_app_dispatch(app, RS_ACTION_X);
    assert(rs_app_standings_mode(app) == RS_STANDINGS_CONSTRUCTORS);
    rs_app_dispatch(app, RS_ACTION_L1);
    assert(rs_app_route(app) == RS_ROUTE_CALENDAR);
    rs_app_dispatch(app, RS_ACTION_START);
    assert(rs_app_overlay(app) == RS_OVERLAY_ABOUT);
    rs_app_dispatch(app, RS_ACTION_B);
    assert(rs_app_overlay(app) == RS_OVERLAY_NONE);
    rs_app_show_disclaimer(app);
    rs_app_dispatch(app, RS_ACTION_A);
    assert(rs_app_take_acknowledgement_request(app));
    assert(rs_app_overlay(app) == RS_OVERLAY_NONE);
    rs_app_dispatch(app,RS_ACTION_START);rs_app_dispatch(app,RS_ACTION_DOWN);assert(rs_app_settings_cursor(app)==1);rs_app_dispatch(app,RS_ACTION_A);assert(rs_app_take_settings_action(app));rs_app_dispatch(app,RS_ACTION_B);
    rs_app_dispatch(app, RS_ACTION_R1);
    rs_app_dispatch(app, RS_ACTION_A);
    rs_app_dispatch(app, RS_ACTION_SELECT);
    assert(rs_app_take_favorite_request(app));
    assert(!rs_app_take_favorite_request(app));
    rs_app_dispatch(app, RS_ACTION_MENU);
    assert(!rs_app_running(app));
    rs_app_destroy(app);
}

static void track_time_offsets_cover_calendar_regions(void){assert(rs_track_utc_offset("spa",1784287800)==7200);assert(rs_track_utc_offset("albert_park",1774000000)==39600);assert(rs_track_utc_offset("vegas",1795200000)==-28800);assert(rs_track_utc_offset("spa",1774744200)==3600);assert(rs_track_utc_offset("spa",1774747800)==7200);assert(rs_track_utc_offset("vegas",1793521800)==-25200);assert(rs_track_utc_offset("vegas",1793529000)==-28800);assert(rs_track_utc_offset("albert_park",1775316600)==39600);assert(rs_track_utc_offset("albert_park",1775320200)==36000);assert(rs_track_utc_offset("albert_park",1791041400)==36000);assert(rs_track_utc_offset("albert_park",1791045000)==39600);}

static void user_sees_complete_driver_standings(const char *fixtures) {
    char path[512];
    char *json;
    RsStandings standings;
    snprintf(path, sizeof(path), "%s/driver_standings.json", fixtures);
    json = read_file(path);
    assert(rs_standings_decode_drivers(json, &standings));
    assert(standings.driver_count >= 20);
    assert(standings.drivers[0].position == 1);
    assert(standings.drivers[0].points >= 0.0);
    assert(standings.drivers[0].family_name[0] != '\0');
    free(json);
}

static void weather_is_aligned_to_the_nearest_session_hour(const char *fixtures) {
    char path[512]; char *json; RsWeatherSnapshot weather; const RsWeatherPoint *point;
    snprintf(path, sizeof(path), "%s/open_meteo.json", fixtures);
    json = read_file(path);
    assert(rs_weather_decode(json, &weather));
    point = rs_weather_nearest(&weather, 1775804400);
    assert(point);
    assert(point->temperature_c == 21.5);
    assert(point->rain_probability == 35);
    free(json);
}

static void weather_keeps_valid_forecast_before_null_tail(const char *fixtures) {
    char path[512]; char *json; RsWeatherSnapshot weather; const RsWeatherPoint *point;
    snprintf(path, sizeof(path), "%s/open_meteo_null_tail.json", fixtures);
    json = read_file(path);
    assert(rs_weather_decode(json, &weather));
    assert(weather.count == 2);
    point = rs_weather_nearest(&weather, 1784289600); /* 2026-07-17 12:00Z */
    assert(point); assert(point->temperature_c == 19.0); assert(point->rain_probability == 25);
    free(json);
}

static void a_snapshot_replaces_the_previous_generation_atomically(void) {
    char dir[256],path[300]; char *loaded;
    snprintf(dir,sizeof(dir),"/tmp/raceslate-store-%ld",(long)getpid());
    snprintf(path,sizeof(path),"%s/snapshot.json",dir);
    assert(rs_store_write_atomic(path,"{\"generation\":1}"));
    assert(rs_store_write_atomic(path,"{\"generation\":2}"));
    loaded=rs_store_read(path); assert(loaded); assert(strcmp(loaded,"{\"generation\":2}")==0); free(loaded);
    unlink(path); rmdir(dir);
}

static void multi_megabyte_season_snapshots_can_be_reloaded(void) {
    char dir[256],path[300];char *payload,*loaded;size_t size=2u*1024u*1024u;
    snprintf(dir,sizeof(dir),"/tmp/raceslate-large-store-%ld",(long)getpid());
    snprintf(path,sizeof(path),"%s/snapshot.json",dir);
    payload=malloc(size+1);assert(payload);memset(payload,'x',size);payload[size]='\0';
    assert(rs_store_write_atomic(path,payload));loaded=rs_store_read(path);assert(loaded);assert(strlen(loaded)==size);
    free(loaded);free(payload);unlink(path);rmdir(dir);
}

static void completed_sessions_expose_full_classifications(const char *fixtures) {
    char path[512]; char *json; RsResultsCatalog catalog={0}; const RsClassification *race;
    snprintf(path,sizeof(path),"%s/results.json",fixtures); json=read_file(path);
    assert(rs_results_decode(json,RS_RESULT_RACE,&catalog)); free(json);
    snprintf(path,sizeof(path),"%s/qualifying.json",fixtures); json=read_file(path);
    assert(rs_results_decode(json,RS_RESULT_QUALIFYING,&catalog)); free(json);
    race=rs_results_find(&catalog,1,RS_RESULT_RACE);
    assert(race); assert(race->entry_count>=20); assert(race->entries[0].position==1);
    assert(race->entries[0].driver_name[0]!='\0'); assert(race->entries[0].points>=0.0);
    assert(race->entries[0].constructor_id[0]!='\0');
    assert(race->entries[0].time[0]!='\0');
    assert(rs_results_find(&catalog,1,RS_RESULT_QUALIFYING));
}

static void current_grid_profiles_include_career_totals_and_chart_series(const char *fixtures) {
    char path[512];
    RsProfileCatalog catalog;
    const RsProfile *driver;
    const RsProfile *constructor;
    snprintf(path, sizeof(path), "%s/profiles.tsv", fixtures);
    assert(rs_profiles_load(path, &catalog));
    driver = rs_profiles_find(&catalog, RS_PROFILE_DRIVER, "max_verstappen");
    assert(driver); assert(driver->starts > 200); assert(driver->wins > 50);
    assert(driver->series_count >= 5); assert(driver->series[0].round == 1);
    constructor = rs_profiles_find(&catalog, RS_PROFILE_CONSTRUCTOR, "ferrari");
    assert(constructor); assert(constructor->championships > 10);
}

static void live_results_rebuild_profile_progression(const char *fixtures) {
    char path[512]; char *json; RsProfileCatalog profiles; RsResultsCatalog results={0}; const RsProfile *driver;
    snprintf(path,sizeof(path),"%s/profiles.tsv",fixtures); assert(rs_profiles_load(path,&profiles));
    snprintf(path,sizeof(path),"%s/results.json",fixtures);json=read_file(path);assert(rs_results_decode(json,RS_RESULT_RACE,&results));free(json);
    snprintf(path,sizeof(path),"%s/sprint.json",fixtures);json=read_file(path);assert(rs_results_decode(json,RS_RESULT_SPRINT,&results));free(json);
    rs_profiles_rebuild_series(&profiles,&results);
    driver=rs_profiles_find(&profiles,RS_PROFILE_DRIVER,"max_verstappen");assert(driver);assert(driver->series_count>0);assert(driver->series[0].points>=0.0);
}

static void historical_standings_build_season_profiles(const char *fixtures){
    char path[512];char *json;RsStandings standings,constructors={0};RsResultsCatalog results={0};RsProfileCatalog profiles;const RsProfile *driver,*team;
    snprintf(path,sizeof(path),"%s/driver_standings.json",fixtures);json=read_file(path);assert(rs_standings_decode_drivers(json,&standings));free(json);
    snprintf(path,sizeof(path),"%s/constructor_standings.json",fixtures);json=read_file(path);assert(rs_standings_decode_constructors(json,&constructors));free(json);standings.constructor_count=constructors.constructor_count;memcpy(standings.constructors,constructors.constructors,sizeof(constructors.constructors));
    snprintf(path,sizeof(path),"%s/results.json",fixtures);json=read_file(path);assert(rs_results_decode(json,RS_RESULT_RACE,&results));free(json);
    snprintf(path,sizeof(path),"%s/qualifying.json",fixtures);json=read_file(path);assert(rs_results_decode(json,RS_RESULT_QUALIFYING,&results));free(json);
    rs_profiles_build_season(&profiles,&standings,&results);
    driver=rs_profiles_find(&profiles,RS_PROFILE_DRIVER,standings.drivers[0].id);assert(driver);assert(driver->season_only);assert(driver->starts>0);assert(driver->series_count>0);
    team=rs_profiles_find(&profiles,RS_PROFILE_CONSTRUCTOR,standings.constructors[0].id);assert(team);assert(team->season_only);assert(team->starts>0);
    snprintf(path,sizeof(path),"%s/profiles.tsv",fixtures);assert(rs_profiles_apply_career(path,&profiles));driver=rs_profiles_find(&profiles,RS_PROFILE_DRIVER,standings.drivers[0].id);assert(driver&&!driver->season_only);assert(driver->starts>20);team=rs_profiles_find(&profiles,RS_PROFILE_CONSTRUCTOR,standings.constructors[0].id);assert(team&&!team->season_only);assert(team->starts>20);
}

static void historical_circuits_use_the_layout_raced_that_round(const char *fixtures){char path[512];RsCircuitAtlas atlas;snprintf(path,sizeof(path),"%s/circuit_atlas.tsv",fixtures);assert(rs_circuit_atlas_load(path,&atlas));assert(strcmp(rs_circuit_atlas_nearest(&atlas,2024,14,50.4372,5.9714),"layout-spa-francorchamps-4")==0);assert(strcmp(rs_circuit_atlas_nearest(&atlas,1950,4,46.95,7.41),"layout-bremgarten-1")==0);assert(strcmp(rs_circuit_atlas_nearest(&atlas,2020,15,26.0325,50.5106),"layout-bahrain-1")==0);assert(strcmp(rs_circuit_atlas_nearest(&atlas,2020,16,26.0325,50.5106),"layout-bahrain-3")==0);assert(rs_circuit_atlas_nearest(&atlas,1951,4,46.95,7.41)==NULL);}

int main(int argc, char **argv) {
    assert(argc == 2);
    user_sees_the_next_session_from_a_jolpica_schedule(argv[1]);
    brick_controls_navigate_the_public_app_state();
    user_sees_complete_driver_standings(argv[1]);
    weather_is_aligned_to_the_nearest_session_hour(argv[1]);
    weather_keeps_valid_forecast_before_null_tail(argv[1]);
    a_snapshot_replaces_the_previous_generation_atomically();
    multi_megabyte_season_snapshots_can_be_reloaded();
    completed_sessions_expose_full_classifications(argv[1]);
    current_grid_profiles_include_career_totals_and_chart_series(argv[1]);
    live_results_rebuild_profile_progression(argv[1]);
    historical_standings_build_season_profiles(argv[1]);
    historical_circuits_use_the_layout_raced_that_round(argv[1]);
    track_time_offsets_cover_calendar_regions();
    puts("ok: core behavior");
    return 0;
}
