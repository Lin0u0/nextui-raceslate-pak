#define _POSIX_C_SOURCE 200809L
#include "rs_app.h"
#include "rs_http.h"
#include "rs_season.h"
#include "rs_standings.h"
#include "rs_store.h"
#include "rs_weather.h"
#include "rs_reference.h"
#include "cJSON.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <curl/curl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_W 1024
#define SCREEN_H 768

typedef struct {
    SDL_mutex *mutex;
    SDL_Thread *thread;
    int running;
    int ready;
    int success;
    char status[160];
    char data_dir[768];
    char ca_file[1024];
    RsSeasonSnapshot season;
    RsStandings standings;
    RsWeatherSnapshot weather;
} RefreshTask;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *display;
    TTF_Font *heading;
    TTF_Font *body;
    TTF_Font *small;
    RsApp *app;
    RsSeasonSnapshot season;
    RsStandings standings;
    RsWeatherSnapshot weather;
    RsReferenceCatalog reference;
    uint32_t last_refresh_at;
    RefreshTask refresh;
    char assets[768];
    char data_dir[768];
    char status[160];
    int64_t now_override;
} Runtime;

static SDL_Color WHITE = {245, 242, 238, 255};
static SDL_Color MUTED = {153, 157, 166, 255};
static SDL_Color RED = {255, 38, 50, 255};
static SDL_Color PANEL = {22, 24, 29, 255};

static int64_t now_utc(const Runtime *runtime) {
    return runtime->now_override > 0 ? runtime->now_override : (int64_t)time(NULL);
}

static void draw_text(Runtime *rt, TTF_Font *font, const char *text, int x, int y,
                      SDL_Color color) {
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect target;
    if (!text || !*text) return;
    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(rt->renderer, surface);
    target = (SDL_Rect){x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (texture) { SDL_RenderCopy(rt->renderer, texture, NULL, &target); SDL_DestroyTexture(texture); }
}

static void fill(Runtime *rt, int x, int y, int w, int h, SDL_Color color) {
    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(rt->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(rt->renderer, &rect);
}

static const char *session_name(RsSessionKind kind) {
    static const char *names[] = {"PRACTICE 1", "PRACTICE 2", "PRACTICE 3", "SPRINT QUALIFYING",
                                  "SPRINT", "QUALIFYING", "RACE"};
    return names[kind];
}

static const RsEvent *event_for_session(const RsSeasonSnapshot *season, const RsSession *session) {
    size_t i, j;
    for (i = 0; i < season->event_count; i++)
        for (j = 0; j < season->events[i].session_count; j++)
            if (&season->events[i].sessions[j] == session) return &season->events[i];
    return NULL;
}

static void format_time(int64_t value, char *out, size_t size) {
    time_t raw = (time_t)value;
    struct tm local;
    localtime_r(&raw, &local);
    strftime(out, size, "%d %b %Y  %H:%M", &local);
    for (char *p = out; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
}

static void draw_header(Runtime *rt) {
    const char *tabs[] = {"NEXT", "CALENDAR", "STANDINGS"};
    RsRoute route = rs_app_route(rt->app);
    int i;
    draw_text(rt, rt->heading, "RACESLATE", 38, 24, WHITE);
    draw_text(rt, rt->small, "INDEPENDENT SEASON COMPANION", 40, 67, MUTED);
    for (i = 0; i < 3; i++) {
        int x = 610 + i * 130;
        draw_text(rt, rt->small, tabs[i], x, 46, i == (int)route ? WHITE : MUTED);
        if (i == (int)route) fill(rt, x, 72, 92, 4, RED);
    }
    fill(rt, 38, 92, 948, 1, (SDL_Color){55, 58, 65, 255});
}

static void draw_track(Runtime *rt, const RsEvent *event) {
    char path[1024];
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect target = {654, 162, 300, 300};
    snprintf(path, sizeof(path), "%s/circuits/%s.bmp", rt->assets, event->circuit_id);
    surface = SDL_LoadBMP(path);
    if (!surface) return;
    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 255, 255, 255));
    texture = SDL_CreateTextureFromSurface(rt->renderer, surface);
    SDL_FreeSurface(surface);
    if (texture) { SDL_SetTextureColorMod(texture, RED.r, RED.g, RED.b); SDL_RenderCopy(rt->renderer, texture, NULL, &target); SDL_DestroyTexture(texture); }
}

static void draw_next(Runtime *rt) {
    const RsSession *next = rs_season_next_session(&rt->season, now_utc(rt));
    const RsEvent *event = event_for_session(&rt->season, next);
    char line[160], time_text[64], countdown[64];
    if (!next || !event) {
        draw_text(rt, rt->display, "SEASON COMPLETE", 42, 150, WHITE);
        draw_text(rt, rt->body, "FINAL STANDINGS REMAIN AVAILABLE", 44, 244, MUTED);
        return;
    }
    snprintf(line, sizeof(line), "ROUND %02d  /  %s", event->round, event->country);
    draw_text(rt, rt->small, line, 44, 128, RED);
    draw_text(rt, rt->display, event->locality, 42, 156, WHITE);
    draw_text(rt, rt->heading, event->name, 45, 248, MUTED);
    draw_text(rt, rt->small, "NEXT SESSION", 46, 318, MUTED);
    draw_text(rt, rt->heading, session_name(next->kind), 44, 343, WHITE);
    format_time(next->starts_at_utc, time_text, sizeof(time_text));
    draw_text(rt, rt->body, time_text, 45, 397, WHITE);
    {
        int64_t delta = next->starts_at_utc - now_utc(rt);
        snprintf(countdown, sizeof(countdown), "%02" PRId64 "D  %02" PRId64 "H  %02" PRId64 "M",
                 delta / 86400, (delta / 3600) % 24, (delta / 60) % 60);
    }
    fill(rt, 42, 466, 538, 112, PANEL);
    draw_text(rt, rt->small, "STARTS IN", 64, 482, MUTED);
    draw_text(rt, rt->heading, countdown, 63, 514, WHITE);
    draw_track(rt, event);
    draw_text(rt, rt->small, event->circuit_name, 654, 484, WHITE);
    snprintf(line, sizeof(line), "%.4f, %.4f  /  YOUR TIME", event->latitude, event->longitude);
    draw_text(rt, rt->small, line, 654, 514, MUTED);
    {
        const RsWeatherPoint *weather = rs_weather_nearest(&rt->weather, next->starts_at_utc);
        if (weather) {
            snprintf(line, sizeof(line), "WEATHER DATA BY OPEN-METEO   %.1f°C   RAIN %d%%   WIND %.1f KM/H",
                     weather->temperature_c, weather->rain_probability, weather->wind_kmh);
        } else snprintf(line, sizeof(line), "WEATHER DATA BY OPEN-METEO  /  FORECAST UNAVAILABLE");
        draw_text(rt, rt->small, line, 44, 635, MUTED);
    }
}

static void draw_calendar(Runtime *rt) {
    int cursor = rs_app_cursor(rt->app);
    int first = cursor > 7 ? cursor - 7 : 0;
    int row;
    char line[180], date_text[64];
    snprintf(line,sizeof(line),"%d RACE CALENDAR",rt->season.season);
    draw_text(rt, rt->heading, line, 42, 122, WHITE);
    for (row = 0; row < 10 && first + row < (int)rt->season.event_count; row++) {
        const RsEvent *event = &rt->season.events[first + row];
        const RsSession *race = &event->sessions[event->session_count - 1];
        int y = 190 + row * 48;
        if (first + row == cursor) fill(rt, 38, y - 5, 948, 42, (SDL_Color){44, 46, 53, 255});
        format_time(race->starts_at_utc, date_text, sizeof(date_text));
        snprintf(line, sizeof(line), "%02d   %-18s  %-28s  %s", event->round, event->country, event->locality, date_text);
        draw_text(rt, rt->body, line, 52, y, first + row == cursor ? WHITE : MUTED);
    }
}

static void draw_standings(Runtime *rt) {
    int cursor = rs_app_cursor(rt->app);
    int first = cursor > 8 ? cursor - 8 : 0;
    int row;
    char line[180];
    bool constructors = rs_app_standings_mode(rt->app) == RS_STANDINGS_CONSTRUCTORS;
    draw_text(rt, rt->heading, constructors ? "CONSTRUCTOR STANDINGS" : "DRIVER STANDINGS", 42, 122, WHITE);
    draw_text(rt, rt->small, "X  SWITCH TABLE", 800, 139, MUTED);
    for (row = 0; row < 11; row++) {
        int index = first + row;
        int y = 188 + row * 45;
        if ((!constructors && index >= (int)rt->standings.driver_count) ||
            (constructors && index >= (int)rt->standings.constructor_count)) break;
        if (index == cursor) fill(rt, 38, y - 5, 948, 40, (SDL_Color){44, 46, 53, 255});
        if (constructors) {
            const RsConstructorStanding *entry = &rt->standings.constructors[index];
            snprintf(line, sizeof(line), "%02d   %-42s  %7.0f PTS   %d WINS", entry->position, entry->name, entry->points, entry->wins);
        } else {
            const RsDriverStanding *entry = &rt->standings.drivers[index];
            snprintf(line, sizeof(line), "%02d   %-3s  %-20s  %-24s  %7.0f PTS", entry->position, entry->code,
                     entry->family_name, entry->constructor_name, entry->points);
        }
        draw_text(rt, rt->body, line, 52, y, index == cursor ? WHITE : MUTED);
    }
}

static void draw_about(Runtime *rt) {
    fill(rt, 160, 116, 704, 536, (SDL_Color){18, 20, 24, 252});
    draw_text(rt, rt->heading, "ABOUT RACESLATE", 204, 154, WHITE);
    draw_text(rt, rt->body, "An unofficial, non-commercial season companion.", 204, 222, WHITE);
    draw_text(rt, rt->small, "Not associated with Formula 1, FIA, teams, drivers, or circuits.", 204, 264, MUTED);
    draw_text(rt, rt->small, "Race data: Jolpica-F1  /  CC BY-NC-SA 4.0", 204, 328, WHITE);
    draw_text(rt, rt->small, "Circuit assets: F1DB  /  CC BY 4.0", 204, 364, WHITE);
    draw_text(rt, rt->small, "Weather: Open-Meteo  /  CC BY 4.0", 204, 400, WHITE);
    draw_text(rt, rt->small, "Fonts: Barlow Condensed + Inter  /  OFL 1.1", 204, 436, WHITE);
    draw_text(rt, rt->small, "B  CLOSE", 204, 574, RED);
}

static void draw_detail(Runtime *rt) {
    fill(rt, 86, 108, 852, 574, (SDL_Color){18, 20, 24, 252});
    if (rs_app_route(rt->app) == RS_ROUTE_CALENDAR && rt->season.event_count) {
        int index = rs_app_cursor(rt->app); char line[512];
        const RsEvent *event; const RsCircuitReference *ref;
        if (index >= (int)rt->season.event_count) index = (int)rt->season.event_count - 1;
        event = &rt->season.events[index]; ref = rs_reference_circuit(&rt->reference, event->circuit_id);
        draw_text(rt, rt->small, "CIRCUIT / HISTORY", 126, 140, RED);
        draw_text(rt, rt->heading, event->circuit_name, 124, 168, WHITE);
        if (ref) {
            snprintf(line,sizeof(line),"%.3f KM    %d TURNS    %s",ref->length_km,ref->turns,ref->direction); draw_text(rt,rt->body,line,126,230,WHITE);
            snprintf(line,sizeof(line),"FIRST CHAMPIONSHIP RACE %d    %d RACES HELD",ref->first_year,ref->races); draw_text(rt,rt->small,line,126,274,MUTED);
            snprintf(line,sizeof(line),"RACE LAP RECORD  %s  /  %s  /  %d",ref->lap_record,ref->record_driver,ref->record_year); draw_text(rt,rt->body,line,126,320,WHITE);
            draw_text(rt,rt->small,"RECENT WINNERS",126,382,MUTED);
            snprintf(line,sizeof(line),"%s",ref->recent_winners); draw_text(rt,rt->small,line,126,416,WHITE);
        }
        draw_track(rt,event);
    } else if (rs_app_route(rt->app) == RS_ROUTE_STANDINGS) {
        int index=rs_app_cursor(rt->app); char line[256];
        bool constructors=rs_app_standings_mode(rt->app)==RS_STANDINGS_CONSTRUCTORS;
        draw_text(rt,rt->small,constructors?"CONSTRUCTOR PROFILE":"DRIVER PROFILE",126,140,RED);
        if(constructors&&index<(int)rt->standings.constructor_count){const RsConstructorStanding *e=&rt->standings.constructors[index];draw_text(rt,rt->display,e->name,124,170,WHITE);snprintf(line,sizeof(line),"P%d   %.0f POINTS   %d WINS",e->position,e->points,e->wins);draw_text(rt,rt->heading,line,126,278,WHITE);}
        else if(!constructors&&index<(int)rt->standings.driver_count){const RsDriverStanding *e=&rt->standings.drivers[index];snprintf(line,sizeof(line),"%s %s",e->given_name,e->family_name);draw_text(rt,rt->display,line,124,170,WHITE);snprintf(line,sizeof(line),"%s  /  %s",e->code,e->constructor_name);draw_text(rt,rt->body,line,128,270,MUTED);snprintf(line,sizeof(line),"P%d   %.0f POINTS   %d WINS",e->position,e->points,e->wins);draw_text(rt,rt->heading,line,126,320,WHITE);}
        draw_text(rt,rt->small,"CURRENT SEASON STATISTICAL PROFILE",126,406,MUTED);
    }
    draw_text(rt,rt->small,"B  CLOSE",126,626,RED);
}

static void render(Runtime *rt) {
    SDL_SetRenderDrawColor(rt->renderer, 9, 10, 13, 255);
    SDL_RenderClear(rt->renderer);
    draw_header(rt);
    if (rs_app_route(rt->app) == RS_ROUTE_NEXT) draw_next(rt);
    else if (rs_app_route(rt->app) == RS_ROUTE_CALENDAR) draw_calendar(rt);
    else draw_standings(rt);
    fill(rt, 38, 710, 948, 1, (SDL_Color){55, 58, 65, 255});
    draw_text(rt, rt->small, rt->status, 42, 724, MUTED);
    draw_text(rt, rt->small, "L1/R1 PAGE   D-PAD NAV   Y REFRESH   START ABOUT   MENU EXIT", 494, 724, MUTED);
    if (rs_app_overlay(rt->app) == RS_OVERLAY_ABOUT) draw_about(rt);
    else if (rs_app_overlay(rt->app) == RS_OVERLAY_DETAIL) draw_detail(rt);
    SDL_RenderPresent(rt->renderer);
}

static char *load_preferred(const char *data_dir, const char *assets, const char *name) {
    char path[1024];
    char *bytes;
    snprintf(path, sizeof(path), "%s/%s", data_dir, name);
    bytes = rs_store_read(path);
    if (bytes) return bytes;
    snprintf(path, sizeof(path), "%s/baseline/%s", assets, name);
    return rs_store_read(path);
}

static bool load_data(Runtime *rt) {
    char snapshot_path[1024]; char *snapshot_bytes; cJSON *snapshot=NULL; char *schedule=NULL,*drivers=NULL,*constructors=NULL;
    snprintf(snapshot_path,sizeof(snapshot_path),"%s/snapshot.json",rt->data_dir);
    snapshot_bytes=rs_store_read(snapshot_path);
    if(snapshot_bytes){snapshot=cJSON_Parse(snapshot_bytes);free(snapshot_bytes);}
    if(snapshot){const cJSON *s=cJSON_GetObjectItemCaseSensitive(snapshot,"schedule"),*d=cJSON_GetObjectItemCaseSensitive(snapshot,"drivers"),*c=cJSON_GetObjectItemCaseSensitive(snapshot,"constructors");if(cJSON_IsObject(s)&&cJSON_IsObject(d)&&cJSON_IsObject(c)){schedule=cJSON_PrintUnformatted(s);drivers=cJSON_PrintUnformatted(d);constructors=cJSON_PrintUnformatted(c);}}
    if(!schedule)schedule=load_preferred(rt->data_dir, rt->assets, "schedule.json");
    if(!drivers)drivers=load_preferred(rt->data_dir, rt->assets, "driver_standings.json");
    if(!constructors)constructors=load_preferred(rt->data_dir, rt->assets, "constructor_standings.json");
    char *weather = load_preferred(rt->data_dir, rt->assets, "weather.json");
    RsStandings constructor_data;
    bool ok = schedule && drivers && constructors && rs_season_decode_schedule(schedule, &rt->season) &&
              rs_standings_decode_drivers(drivers, &rt->standings) &&
              rs_standings_decode_constructors(constructors, &constructor_data);
    if (ok) {
        rt->standings.constructor_count = constructor_data.constructor_count;
        memcpy(rt->standings.constructors, constructor_data.constructors, sizeof(constructor_data.constructors));
        if (weather) rs_weather_decode(weather, &rt->weather);
    }
    free(schedule); free(drivers); free(constructors); free(weather); cJSON_Delete(snapshot);
    return ok;
}

static int refresh_thread(void *context) {
    RefreshTask *task = context;
    RsHttpResponse schedule = {0}, drivers = {0}, constructors = {0};
    RsSeasonSnapshot season;
    RsStandings driver_data, constructor_data;
    RsWeatherSnapshot weather = {0};
    bool ok = rs_http_get_https("https://api.jolpi.ca/ergast/f1/current.json?limit=100", task->ca_file, &schedule) &&
              rs_season_decode_schedule(schedule.bytes, &season);
    if (ok) {
        char url[256];
        snprintf(url,sizeof(url),"https://api.jolpi.ca/ergast/f1/%d/driverstandings.json?limit=100",season.season);
        ok=rs_http_get_https(url,task->ca_file,&drivers)&&rs_standings_decode_drivers(drivers.bytes,&driver_data);
        snprintf(url,sizeof(url),"https://api.jolpi.ca/ergast/f1/%d/constructorstandings.json?limit=100",season.season);
        ok=ok&&rs_http_get_https(url,task->ca_file,&constructors)&&rs_standings_decode_constructors(constructors.bytes,&constructor_data);
    }
    if (ok) {
        const RsSession *next = rs_season_next_session(&season, (int64_t)time(NULL));
        const RsEvent *event = event_for_session(&season, next);
        if (event && next) {
            char url[768]; RsHttpResponse weather_response = {0};
            snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f&hourly=temperature_2m,precipitation_probability,wind_speed_10m&timezone=UTC&wind_speed_unit=kmh&forecast_days=16",
                     event->latitude, event->longitude);
            if (rs_http_get_https(url, task->ca_file, &weather_response) && rs_weather_decode(weather_response.bytes, &weather)) {
                char path[1024]; snprintf(path, sizeof(path), "%s/weather.json", task->data_dir);
                rs_store_write_atomic(path, weather_response.bytes);
            }
            rs_http_response_dispose(&weather_response);
        }
    }
    if (ok) {
        char path[1024]; size_t length=schedule.length+drivers.length+constructors.length+64; char *generation=malloc(length);
        if(!generation)ok=false;
        else{snprintf(generation,length,"{\"schedule\":%s,\"drivers\":%s,\"constructors\":%s}",schedule.bytes,drivers.bytes,constructors.bytes);snprintf(path,sizeof(path),"%s/snapshot.json",task->data_dir);ok=rs_store_write_atomic(path,generation);free(generation);}
    }
    SDL_LockMutex(task->mutex);
    if (ok) {
        task->season = season;
        task->standings = driver_data;
        task->standings.constructor_count = constructor_data.constructor_count;
        task->weather = weather;
        memcpy(task->standings.constructors, constructor_data.constructors, sizeof(constructor_data.constructors));
        snprintf(task->status, sizeof(task->status), "ONLINE DATA UPDATED");
    } else snprintf(task->status, sizeof(task->status), "REFRESH FAILED — USING LAST COMPLETE SNAPSHOT");
    task->success = ok;
    task->ready = 1;
    task->running = 0;
    SDL_UnlockMutex(task->mutex);
    rs_http_response_dispose(&schedule); rs_http_response_dispose(&drivers); rs_http_response_dispose(&constructors);
    return 0;
}

static void start_refresh(Runtime *rt) {
    uint32_t now=SDL_GetTicks();
    SDL_LockMutex(rt->refresh.mutex);
    if (rt->refresh.running) { SDL_UnlockMutex(rt->refresh.mutex); return; }
    if (rt->last_refresh_at && now-rt->last_refresh_at<60000) { snprintf(rt->status,sizeof(rt->status),"REFRESH COOLDOWN — LAST REQUEST WAS RECENT"); SDL_UnlockMutex(rt->refresh.mutex); return; }
    rt->refresh.running = 1; rt->refresh.ready = 0; rt->last_refresh_at=now;
    SDL_UnlockMutex(rt->refresh.mutex);
    snprintf(rt->status, sizeof(rt->status), "REFRESHING VERIFIED HTTPS DATA…");
    rt->refresh.thread=SDL_CreateThread(refresh_thread, "raceslate-refresh", &rt->refresh);
    if(!rt->refresh.thread){SDL_LockMutex(rt->refresh.mutex);rt->refresh.running=0;SDL_UnlockMutex(rt->refresh.mutex);snprintf(rt->status,sizeof(rt->status),"REFRESH WORKER COULD NOT START");}
}

static RsAction map_event(const SDL_Event *event) {
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_UP: return RS_ACTION_UP; case SDLK_DOWN: return RS_ACTION_DOWN;
            case SDLK_LEFT: return RS_ACTION_LEFT; case SDLK_RIGHT: return RS_ACTION_RIGHT;
            case SDLK_RETURN: return RS_ACTION_A; case SDLK_ESCAPE: return RS_ACTION_B;
            case SDLK_x: return RS_ACTION_X; case SDLK_y: return RS_ACTION_Y;
            case SDLK_q: return RS_ACTION_L1; case SDLK_e: return RS_ACTION_R1;
            case SDLK_a: return RS_ACTION_L2; case SDLK_d: return RS_ACTION_R2;
            case SDLK_SPACE: return RS_ACTION_SELECT; case SDLK_s: return RS_ACTION_START;
            case SDLK_m: return RS_ACTION_MENU; default: break;
        }
    }
    if (event->type == SDL_JOYBUTTONDOWN) {
        static const RsAction actions[] = {RS_ACTION_B, RS_ACTION_A, RS_ACTION_Y, RS_ACTION_X,
            RS_ACTION_L1, RS_ACTION_R1, RS_ACTION_SELECT, RS_ACTION_START, RS_ACTION_MENU,
            RS_ACTION_L2, RS_ACTION_R2};
        if (event->jbutton.button < sizeof(actions) / sizeof(actions[0])) return actions[event->jbutton.button];
    }
    if (event->type == SDL_JOYHATMOTION) {
        if (event->jhat.value & SDL_HAT_UP) return RS_ACTION_UP;
        if (event->jhat.value & SDL_HAT_DOWN) return RS_ACTION_DOWN;
        if (event->jhat.value & SDL_HAT_LEFT) return RS_ACTION_LEFT;
        if (event->jhat.value & SDL_HAT_RIGHT) return RS_ACTION_RIGHT;
    }
    return RS_ACTION_NONE;
}

int main(int argc, char **argv) {
    Runtime rt = {0};
    SDL_Event event;
    const char *screenshot = NULL;
    int offline = 0, i;
    snprintf(rt.assets, sizeof(rt.assets), "assets");
    snprintf(rt.data_dir, sizeof(rt.data_dir), "data");
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--assets") && i + 1 < argc) snprintf(rt.assets, sizeof(rt.assets), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(rt.data_dir, sizeof(rt.data_dir), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--now") && i + 1 < argc) rt.now_override = strtoll(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!strcmp(argv[i], "--offline")) offline = 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0 || TTF_Init() != 0 || curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return 1;
    rt.window = SDL_CreateWindow("RaceSlate", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    rt.renderer = SDL_CreateRenderer(rt.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!rt.renderer) rt.renderer = SDL_CreateRenderer(rt.window, -1, SDL_RENDERER_SOFTWARE);
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/fonts/BarlowCondensed-SemiBold.ttf", rt.assets);
        rt.display = TTF_OpenFont(path, 86); rt.heading = TTF_OpenFont(path, 42);
        snprintf(path, sizeof(path), "%s/fonts/Inter.ttf", rt.assets);
        rt.body = TTF_OpenFont(path, 22); rt.small = TTF_OpenFont(path, 15);
    }
    rt.app = rs_app_create();
    rt.refresh.mutex = SDL_CreateMutex();
    snprintf(rt.refresh.data_dir, sizeof(rt.refresh.data_dir), "%s", rt.data_dir);
    snprintf(rt.refresh.ca_file, sizeof(rt.refresh.ca_file), "%s/cacert.pem", rt.assets);
    snprintf(rt.status, sizeof(rt.status), "BASELINE DATA · Y TO REFRESH");
    {
        char path[1024]; snprintf(path,sizeof(path),"%s/reference/history.tsv",rt.assets);
        if (!rs_reference_load(path,&rt.reference)) return 2;
    }
    if (!rt.window || !rt.renderer || !rt.display || !rt.heading || !rt.body || !rt.small || !rt.app || !load_data(&rt)) return 2;
    if (offline) snprintf(rt.status,sizeof(rt.status),"OFFLINE BASELINE DATA");
    render(&rt);
    if (screenshot) { SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(rt.renderer, NULL, SDL_PIXELFORMAT_ARGB8888, shot->pixels, shot->pitch); SDL_SaveBMP(shot, screenshot); SDL_FreeSurface(shot); rs_app_dispatch(rt.app, RS_ACTION_MENU); }
    while (rs_app_running(rt.app)) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) rs_app_dispatch(rt.app, RS_ACTION_MENU);
            else { RsAction action = map_event(&event); if (action != RS_ACTION_NONE) rs_app_dispatch(rt.app, action); }
        }
        if (rs_app_take_refresh_request(rt.app)) start_refresh(&rt);
        SDL_LockMutex(rt.refresh.mutex);
        if (rt.refresh.ready) { if (rt.refresh.success) { rt.season = rt.refresh.season; rt.standings = rt.refresh.standings; rt.weather = rt.refresh.weather; }
            snprintf(rt.status, sizeof(rt.status), "%s", rt.refresh.status); rt.refresh.ready = 0; if(rt.refresh.thread){SDL_DetachThread(rt.refresh.thread);rt.refresh.thread=NULL;} }
        SDL_UnlockMutex(rt.refresh.mutex);
        render(&rt);
        SDL_Delay(16);
    }
    if(rt.refresh.thread) SDL_WaitThread(rt.refresh.thread,NULL);
    TTF_CloseFont(rt.display); TTF_CloseFont(rt.heading); TTF_CloseFont(rt.body); TTF_CloseFont(rt.small);
    SDL_DestroyMutex(rt.refresh.mutex); rs_app_destroy(rt.app); SDL_DestroyRenderer(rt.renderer); SDL_DestroyWindow(rt.window);
    curl_global_cleanup(); TTF_Quit(); SDL_Quit();
    return 0;
}
