#define _POSIX_C_SOURCE 200809L
#include "rs_app.h"
#include "rs_http.h"
#include "rs_season.h"
#include "rs_standings.h"
#include "rs_store.h"
#include "rs_weather.h"
#include "rs_reference.h"
#include "rs_results.h"
#include "rs_profiles.h"
#include "rs_timezone.h"
#include "cJSON.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <curl/curl.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    RsResultsCatalog results;
    int weather_live;
} RefreshTask;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *display;
    TTF_Font *heading;
    TTF_Font *body;
    TTF_Font *small;
    TTF_Font *metric;
    RsApp *app;
    RsSeasonSnapshot season;
    RsStandings standings;
    RsWeatherSnapshot weather;
    RsResultsCatalog results;
    RsReferenceCatalog reference;
    RsProfileCatalog profiles;
    uint32_t last_refresh_at;
    RefreshTask refresh;
    char assets[768];
    char data_dir[768];
    char status[160];
    char favorite_driver[48];
    char favorite_constructor[48];
    bool first_launch;
    bool haptics;
    bool haptic_available;
    bool weather_live;
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

static void draw_text_center_y(Runtime *rt, TTF_Font *font, const char *text, int x, int top, int height, SDL_Color color) {
    int text_height = 0;
    if (text && TTF_SizeUTF8(font, text, NULL, &text_height) == 0) draw_text(rt, font, text, x, top + (height - text_height) / 2, color);
}

static void draw_text_right_center_y(Runtime *rt, TTF_Font *font, const char *text, int right, int top, int height, SDL_Color color) {
    int width = 0, text_height = 0;
    if (text && TTF_SizeUTF8(font, text, &width, &text_height) == 0) draw_text(rt, font, text, right - width, top + (height - text_height) / 2, color);
}

static void format_points(double points, char *output, size_t capacity) {
    if (fabs(points - round(points)) < 0.001) snprintf(output, capacity, "%.0f", points);
    else snprintf(output, capacity, "%.1f", points);
}

static SDL_Color team_color(const char *id) {
    if (!id) return MUTED;
    if (!strcmp(id,"ferrari")) return (SDL_Color){232,0,45,255};
    if (!strcmp(id,"mclaren")) return (SDL_Color){255,128,0,255};
    if (!strcmp(id,"mercedes")) return (SDL_Color){0,215,182,255};
    if (!strcmp(id,"red_bull")) return (SDL_Color){54,113,198,255};
    if (!strcmp(id,"aston_martin")) return (SDL_Color){34,153,113,255};
    if (!strcmp(id,"alpine")) return (SDL_Color){255,135,188,255};
    if (!strcmp(id,"williams")) return (SDL_Color){0,160,222,255};
    if (!strcmp(id,"haas")) return (SDL_Color){182,186,189,255};
    if (!strcmp(id,"rb")) return (SDL_Color){102,146,255,255};
    if (!strcmp(id,"audi")) return (SDL_Color){217,255,0,255};
    if (!strcmp(id,"cadillac")) return (SDL_Color){212,175,55,255};
    return MUTED;
}

static void classification_result(const RsClassification *classification, size_t index, char *output, size_t capacity) {
    const RsClassificationEntry *entry = &classification->entries[index];
    if (classification->kind == RS_RESULT_QUALIFYING) {
        const char *lap = entry->q3[0] ? entry->q3 : entry->q2[0] ? entry->q2 : entry->q1;
        snprintf(output, capacity, "%s", lap[0] ? lap : entry->status);
    } else if (index == 0 && entry->time[0]) snprintf(output, capacity, "%s", entry->time);
    else if (classification->entry_count > 0 && classification->entries[0].laps > entry->laps) {
        int gap = classification->entries[0].laps - entry->laps;
        snprintf(output, capacity, "+%d %s", gap, gap == 1 ? "LAP" : "LAPS");
    } else if (entry->time[0]) snprintf(output, capacity, "%s%s", entry->time[0] != '+' ? "+" : "", entry->time);
    else snprintf(output, capacity, "%s", entry->status);
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

static void format_event_time(const Runtime *rt,const RsEvent *event,int64_t value,char *out,size_t size){if(!rs_app_track_time(rt->app)){format_time(value,out,size);return;}time_t raw=(time_t)(value+rs_track_utc_offset(event->circuit_id,value));struct tm track;gmtime_r(&raw,&track);strftime(out,size,"%d %b %Y  %H:%M",&track);for(char *p=out;*p;p++)if(*p>='a'&&*p<='z')*p=(char)(*p-'a'+'A');}

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

static void draw_track(Runtime *rt, const RsEvent *event, SDL_Rect target) {
    char path[1024];
    SDL_Surface *surface;
    SDL_Texture *texture;
    snprintf(path, sizeof(path), "%s/circuits/%s.bmp", rt->assets, event->circuit_id);
    surface = SDL_LoadBMP(path);
    if (!surface) return;
    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));
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
    format_event_time(rt,event,next->starts_at_utc, time_text, sizeof(time_text));
    draw_text(rt, rt->body, time_text, 45, 397, WHITE);
    {
        int64_t delta = next->starts_at_utc - now_utc(rt);
        snprintf(countdown, sizeof(countdown), "%02" PRId64 "D  %02" PRId64 "H  %02" PRId64 "M",
                 delta / 86400, (delta / 3600) % 24, (delta / 60) % 60);
    }
    fill(rt, 42, 466, 538, 184, PANEL);
    draw_text(rt, rt->small, "STARTS IN", 64, 482, MUTED);
    draw_text(rt, rt->heading, countdown, 63, 514, WHITE);
    fill(rt,64,580,494,1,(SDL_Color){55,58,65,255});
    {size_t session_index;const RsSession *following=NULL;for(session_index=0;session_index<event->session_count;session_index++)if(event->sessions[session_index].starts_at_utc>next->starts_at_utc){following=&event->sessions[session_index];break;}draw_text(rt,rt->small,"FOLLOWING",64,594,MUTED);if(following){format_event_time(rt,event,following->starts_at_utc,time_text,sizeof(time_text));snprintf(line,sizeof(line),"%s  /  %s",session_name(following->kind),time_text);draw_text(rt,rt->small,line,64,620,WHITE);}else draw_text(rt,rt->small,"FINAL SESSION OF THE WEEKEND",64,620,WHITE);}
    draw_track(rt, event,(SDL_Rect){620,122,360,360});
    draw_text(rt, rt->small, event->circuit_name, 654, 486, WHITE);
    snprintf(line, sizeof(line), "%.4f, %.4f  /  %s", event->latitude, event->longitude,rs_app_track_time(rt->app)?"TRACK TIME":"YOUR TIME");
    draw_text(rt, rt->small, line, 654, 516, MUTED);
    {
        const RsWeatherPoint *weather = rs_weather_nearest(&rt->weather, next->starts_at_utc);
        fill(rt, 620, 558, 366, 92, PANEL);
        snprintf(line,sizeof(line),"SESSION FORECAST  /  OPEN-METEO  /  %s",weather?(rt->weather_live?"LIVE":"CACHED"):(rt->weather.count?"OUT OF RANGE":"NO DATA"));
        draw_text(rt, rt->small, line, 640, 572, MUTED);
        if (weather) {
            draw_text(rt,rt->small,"TEMP",640,601,MUTED);snprintf(line, sizeof(line), "%.1f°C",weather->temperature_c);draw_text(rt,rt->metric,line,640,619,WHITE);
            draw_text(rt,rt->small,"RAIN",758,601,MUTED);snprintf(line, sizeof(line), "%d%%",weather->rain_probability);draw_text(rt,rt->metric,line,758,619,WHITE);
            draw_text(rt,rt->small,"WIND KM/H",856,601,MUTED);snprintf(line, sizeof(line), "%.1f",weather->wind_kmh);draw_text(rt,rt->metric,line,856,619,WHITE);
        } else draw_text(rt, rt->metric, "UNAVAILABLE  /  PRESS Y", 640, 608, MUTED);
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
        format_event_time(rt,event,race->starts_at_utc, date_text, sizeof(date_text));
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
            snprintf(line, sizeof(line), "%s %02d   %-40s  %7.0f PTS   %d WINS", !strcmp(entry->id,rt->favorite_constructor)?"*":" ", entry->position, entry->name, entry->points, entry->wins);
        } else {
            const RsDriverStanding *entry = &rt->standings.drivers[index];
            snprintf(line, sizeof(line), "%s %02d   %-3s  %-20s  %-22s  %7.0f PTS", !strcmp(entry->id,rt->favorite_driver)?"*":" ", entry->position, entry->code,
                     entry->family_name, entry->constructor_name, entry->points);
        }
        draw_text(rt, rt->body, line, 52, y, index == cursor ? WHITE : MUTED);
    }
}

static void draw_about(Runtime *rt) {
    fill(rt, 160, 116, 704, 536, (SDL_Color){18, 20, 24, 252});
    draw_text(rt, rt->heading, rt->first_launch ? "WELCOME TO RACESLATE" : "SETTINGS & ABOUT", 204, 154, WHITE);
    draw_text(rt, rt->body, "An unofficial, non-commercial season companion.", 204, 222, WHITE);
    draw_text(rt, rt->small, "Not associated with Formula 1, FIA, teams, drivers, or circuits.", 204, 264, MUTED);
    if(!rt->first_launch){int cursor=rs_app_settings_cursor(rt->app),row;const char *haptic_label=rt->haptic_available?(rt->haptics?"HAPTICS                         ON":"HAPTICS                         OFF"):"HAPTICS                 UNSUPPORTED";const char *labels[3]={haptic_label,"CLEAR DOWNLOADED CACHE","LICENSES & ATTRIBUTION"};for(row=0;row<3;row++){int y=310+row*48;if(row==cursor)fill(rt,196,y-7,632,40,(SDL_Color){44,46,53,255});draw_text(rt,rt->body,labels[row],212,y,row==cursor?WHITE:MUTED);}draw_text(rt,rt->small,"JOLPICA CC BY-NC-SA · F1DB / OPEN-METEO CC BY · FONTS OFL",204,480,MUTED);draw_text(rt,rt->small,"A SELECT   B CLOSE",204,574,RED);return;}
    draw_text(rt, rt->small, "Race data: Jolpica-F1  /  CC BY-NC-SA 4.0", 204, 328, WHITE);
    draw_text(rt, rt->small, "Circuit assets: F1DB  /  CC BY 4.0", 204, 364, WHITE);
    draw_text(rt, rt->small, "Weather: Open-Meteo  /  CC BY 4.0", 204, 400, WHITE);
    draw_text(rt, rt->small, "Fonts: Barlow Condensed + Inter  /  OFL 1.1", 204, 436, WHITE);
    draw_text(rt, rt->small, rt->first_launch ? "A  ACCEPT AND CONTINUE" : "A / B  CLOSE", 204, 574, RED);
}

static void draw_profile_chart(Runtime *rt, const RsProfile *profile, bool points_mode) {
    const int x = 126, y = 436, width = 746, height = 130;
    size_t index;
    double maximum = 1.0;
    char label[80];
    if (!profile || profile->series_count < 2) return;
    for (index = 0; index < profile->series_count; index++) {
        double value = points_mode ? profile->series[index].points : profile->series[index].position;
        if (value > maximum) maximum = value;
    }
    SDL_SetRenderDrawColor(rt->renderer, 60, 63, 70, 255);
    SDL_RenderDrawRect(rt->renderer, &(SDL_Rect){x, y, width, height});
    SDL_SetRenderDrawColor(rt->renderer, RED.r, RED.g, RED.b, RED.a);
    for (index = 1; index < profile->series_count; index++) {
        const RsProfilePoint *previous = &profile->series[index - 1], *current = &profile->series[index];
        double pv = points_mode ? previous->points : previous->position;
        double cv = points_mode ? current->points : current->position;
        int x1 = x + (int)((index - 1) * (size_t)width / (profile->series_count - 1));
        int x2 = x + (int)(index * (size_t)width / (profile->series_count - 1));
        int y1 = points_mode ? y + height - (int)(pv / maximum * height) : y + (int)((pv - 1.0) / maximum * height);
        int y2 = points_mode ? y + height - (int)(cv / maximum * height) : y + (int)((cv - 1.0) / maximum * height);
        SDL_RenderDrawLine(rt->renderer, x1, y1, x2, y2);
        fill(rt, x2 - 2, y2 - 2, 5, 5, WHITE);
        { char value[24]; if(points_mode)format_points(cv,value,sizeof(value));else snprintf(value,sizeof(value),"%d",current->position);draw_text(rt,rt->small,value,x2-6,y2>y+18?y2-20:y2+5,WHITE); }
    }
    {char value[24];const RsProfilePoint *first=&profile->series[0];double fv=points_mode?first->points:first->position;int fy=points_mode?y+height-(int)(fv/maximum*height):y+(int)((fv-1.0)/maximum*height);if(points_mode)format_points(fv,value,sizeof(value));else snprintf(value,sizeof(value),"%d",first->position);fill(rt,x-2,fy-2,5,5,WHITE);draw_text(rt,rt->small,value,x,fy>y+18?fy-20:fy+5,WHITE);}
    snprintf(label, sizeof(label), "%s  /  X SWITCH METRIC", points_mode ? "POINTS PROGRESSION" : "CHAMPIONSHIP POSITION");
    draw_text(rt, rt->small, label, x, y - 27, MUTED);
}

static int split_history(char *text,char **items,int capacity){int count=0;char *save=NULL,*item=strtok_r(text,"|",&save);while(item&&count<capacity){items[count++]=item;item=strtok_r(NULL,"|",&save);}return count;}
static void draw_history_log(Runtime *rt,const RsCircuitReference *ref,int cursor){char winners[3072],poles[3072],*winner_items[96],*pole_items[96],line[64];int winner_count,pole_count,pages,page,row;snprintf(winners,sizeof(winners),"%s",ref->all_winners);snprintf(poles,sizeof(poles),"%s",ref->all_poles);winner_count=split_history(winners,winner_items,96);pole_count=split_history(poles,pole_items,96);pages=((winner_count>pole_count?winner_count:pole_count)+7)/8;if(pages<1)pages=1;page=(cursor-1)%pages;snprintf(line,sizeof(line),"VENUE ARCHIVE  /  PAGE %d OF %d  /  UP DOWN",page+1,pages);draw_text(rt,rt->small,line,126,140,RED);draw_text(rt,rt->heading,"WINNERS & POLES",124,168,WHITE);draw_text(rt,rt->small,"RACE WINNER",126,230,MUTED);draw_text(rt,rt->small,"POLE POSITION",520,230,MUTED);for(row=0;row<8;row++){int index=page*8+row,y=266+row*42;if(index<winner_count)draw_text(rt,rt->body,winner_items[index],126,y,WHITE);if(index<pole_count)draw_text(rt,rt->body,pole_items[index],520,y,WHITE);}}

static void draw_detail(Runtime *rt) {
    fill(rt, 86, 108, 852, 574, (SDL_Color){18, 20, 24, 252});
    if (rs_app_route(rt->app) == RS_ROUTE_CALENDAR && rt->season.event_count) {
        int index = rs_app_cursor(rt->app); char line[512];
        const RsEvent *event; const RsCircuitReference *ref;
        if (index >= (int)rt->season.event_count) index = (int)rt->season.event_count - 1;
        event = &rt->season.events[index]; ref = rs_reference_circuit(&rt->reference, event->circuit_id);
        if(rs_app_detail_mode(rt->app)==RS_DETAIL_HISTORY){
         if(ref&&rs_app_detail_cursor(rt->app)>0)draw_history_log(rt,ref,rs_app_detail_cursor(rt->app));else{
         draw_text(rt, rt->small, "HISTORY  /  X NEXT VIEW", 126, 140, RED);
         draw_text(rt, rt->heading, event->circuit_name, 124, 168, WHITE);
         if (ref) {
            snprintf(line,sizeof(line),"%.3f KM    %d TURNS    %s",ref->length_km,ref->turns,ref->direction); draw_text(rt,rt->body,line,126,230,WHITE);
            snprintf(line,sizeof(line),"FIRST CHAMPIONSHIP RACE %d    %d RACES HELD",ref->first_year,ref->races); draw_text(rt,rt->small,line,126,274,MUTED);
            snprintf(line,sizeof(line),"RACE LAP RECORD  %s  /  %s  /  %d",ref->lap_record,ref->record_driver,ref->record_year); draw_text(rt,rt->body,line,126,320,WHITE);
            snprintf(line,sizeof(line),"MOST WINS  %s    MOST POLES  %s",ref->most_wins,ref->most_poles);draw_text(rt,rt->small,line,126,360,WHITE);
            draw_text(rt,rt->small,"RECENT WINNERS  /  DOWN FULL ARCHIVE",126,388,MUTED);
            snprintf(line,sizeof(line),"%s",ref->recent_winners); draw_text(rt,rt->small,line,126,408,WHITE);
         }
         {size_t session_index;draw_text(rt,rt->small,"WEEKEND SCHEDULE",126,438,MUTED);for(session_index=0;session_index<event->session_count;session_index++){char when[64];const RsSession *session=&event->sessions[session_index];format_event_time(rt,event,session->starts_at_utc,when,sizeof(when));snprintf(line,sizeof(line),"%-19s  %s  /  %s",session_name(session->kind),when,session->starts_at_utc<=now_utc(rt)?"COMPLETE":"UPCOMING");draw_text(rt,rt->small,line,126,462+(int)session_index*27,session->starts_at_utc<=now_utc(rt)?MUTED:WHITE);}}
         draw_track(rt,event,(SDL_Rect){620,150,290,290});
         }
        }else{
         RsResultKind kind=rs_app_detail_mode(rt->app)==RS_DETAIL_RACE?RS_RESULT_RACE:rs_app_detail_mode(rt->app)==RS_DETAIL_QUALIFYING?RS_RESULT_QUALIFYING:RS_RESULT_SPRINT;
         const RsClassification *classification=rs_results_find(&rt->results,event->round,kind);int cursor=rs_app_detail_cursor(rt->app),first=cursor>8?cursor-8:0,row;
         const char *title=kind==RS_RESULT_RACE?"RACE CLASSIFICATION":kind==RS_RESULT_QUALIFYING?"QUALIFYING CLASSIFICATION":"SPRINT CLASSIFICATION";
         draw_text(rt,rt->small,"RESULTS  /  X NEXT VIEW",126,140,RED);draw_text(rt,rt->heading,title,124,168,WHITE);
         if(!classification)draw_text(rt,rt->body,"RESULT UNAVAILABLE",126,244,MUTED);
         else {draw_text(rt,rt->small,"POS   DRIVER       CONSTRUCTOR",126,220,MUTED);draw_text(rt,rt->small,kind==RS_RESULT_QUALIFYING?"LAP TIME":"TIME / GAP",786,220,MUTED);for(row=0;row<10&&first+row<(int)classification->entry_count;row++){const RsClassificationEntry *e=&classification->entries[first+row];int y=248+row*38,row_top=y-3,row_height=34;SDL_Color text_color=first+row==cursor?WHITE:MUTED;char position[8],points[24],result[64];if(first+row==cursor)fill(rt,116,row_top,790,row_height,(SDL_Color){44,46,53,255});fill(rt,126,row_top+(row_height-18)/2,4,18,team_color(e->constructor_id));snprintf(position,sizeof(position),"%02d",e->position);format_points(e->points,points,sizeof(points));classification_result(classification,(size_t)(first+row),result,sizeof(result));draw_text_right_center_y(rt,rt->small,position,166,row_top,row_height,text_color);draw_text_center_y(rt,rt->small,e->driver_code,198,row_top,row_height,text_color);draw_text_center_y(rt,rt->small,e->constructor_name,286,row_top,row_height,team_color(e->constructor_id));draw_text_right_center_y(rt,rt->small,points,670,row_top,row_height,text_color);draw_text_center_y(rt,rt->small,"PTS",680,row_top,row_height,MUTED);draw_text_right_center_y(rt,rt->small,result,890,row_top,row_height,text_color);}}
        }
    } else if (rs_app_route(rt->app) == RS_ROUTE_STANDINGS) {
        int index=rs_app_cursor(rt->app); char line[256];
        bool constructors=rs_app_standings_mode(rt->app)==RS_STANDINGS_CONSTRUCTORS;
        const RsProfile *profile=NULL;
        draw_text(rt,rt->small,constructors?"CONSTRUCTOR PROFILE":"DRIVER PROFILE",126,140,RED);
        if(constructors&&index<(int)rt->standings.constructor_count){const RsConstructorStanding *e=&rt->standings.constructors[index];profile=rs_profiles_find(&rt->profiles,RS_PROFILE_CONSTRUCTOR,e->id);draw_text(rt,rt->display,e->name,124,170,WHITE);snprintf(line,sizeof(line),"P%d   %.0f POINTS   %d WINS",e->position,e->points,e->wins);draw_text(rt,rt->heading,line,126,278,WHITE);}
        else if(!constructors&&index<(int)rt->standings.driver_count){const RsDriverStanding *e=&rt->standings.drivers[index];profile=rs_profiles_find(&rt->profiles,RS_PROFILE_DRIVER,e->id);snprintf(line,sizeof(line),"%s %s",e->given_name,e->family_name);draw_text(rt,rt->display,line,124,170,WHITE);snprintf(line,sizeof(line),"%s  /  %s",e->code,e->constructor_name);draw_text(rt,rt->body,line,128,270,MUTED);snprintf(line,sizeof(line),"P%d   %.0f POINTS   %d WINS",e->position,e->points,e->wins);draw_text(rt,rt->heading,line,126,320,WHITE);}
        if(profile){snprintf(line,sizeof(line),"CAREER  %d STARTS   %d WINS   %d PODIUMS   %d POLES   %d TITLES",profile->starts,profile->wins,profile->podiums,profile->poles,profile->championships);draw_text(rt,rt->small,line,126,382,WHITE);draw_profile_chart(rt,profile,(rs_app_detail_mode(rt->app)%2)==RS_DETAIL_RACE);draw_text(rt,rt->small,"SELECT  SET FAVORITE",680,626,RED);}
        else draw_text(rt,rt->small,"CAREER PROFILE UNAVAILABLE",126,406,MUTED);
    }
    draw_text(rt,rt->small,"B  CLOSE",126,650,RED);
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
    draw_text(rt, rt->small, rs_app_route(rt->app)==RS_ROUTE_NEXT?"X TIMEZONE   L1/R1 PAGE   Y REFRESH   START SETTINGS   MENU EXIT":"L1/R1 PAGE   D-PAD NAV   Y REFRESH   START SETTINGS   MENU EXIT", 494, 724, MUTED);
    if (rs_app_overlay(rt->app) == RS_OVERLAY_ABOUT||rs_app_overlay(rt->app)==RS_OVERLAY_DISCLAIMER) draw_about(rt);
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
    char snapshot_path[1024]; char *snapshot_bytes; cJSON *snapshot=NULL; char *schedule=NULL,*drivers=NULL,*constructors=NULL,*results=NULL,*qualifying=NULL,*sprint=NULL,*weather=NULL;
    bool snapshot_valid=false;snprintf(snapshot_path,sizeof(snapshot_path),"%s/snapshot.json",rt->data_dir);
    snapshot_bytes=rs_store_read(snapshot_path);
    if(snapshot_bytes){snapshot=cJSON_Parse(snapshot_bytes);free(snapshot_bytes);}
    if(snapshot){const cJSON *s=cJSON_GetObjectItemCaseSensitive(snapshot,"schedule"),*d=cJSON_GetObjectItemCaseSensitive(snapshot,"drivers"),*c=cJSON_GetObjectItemCaseSensitive(snapshot,"constructors"),*w=cJSON_GetObjectItemCaseSensitive(snapshot,"weather");if(cJSON_IsObject(s)&&cJSON_IsObject(d)&&cJSON_IsObject(c)){snapshot_valid=true;schedule=cJSON_PrintUnformatted(s);drivers=cJSON_PrintUnformatted(d);constructors=cJSON_PrintUnformatted(c);results=cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(snapshot,"results"));qualifying=cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(snapshot,"qualifying"));sprint=cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(snapshot,"sprint"));if(cJSON_IsObject(w))weather=cJSON_PrintUnformatted(w);}}
    if(!schedule)schedule=load_preferred(rt->data_dir, rt->assets, "schedule.json");
    if(!drivers)drivers=load_preferred(rt->data_dir, rt->assets, "driver_standings.json");
    if(!constructors)constructors=load_preferred(rt->data_dir, rt->assets, "constructor_standings.json");
    if(!results)results=load_preferred(rt->data_dir,rt->assets,"results.json");
    if(!qualifying)qualifying=load_preferred(rt->data_dir,rt->assets,"qualifying.json");
    if(!sprint)sprint=load_preferred(rt->data_dir,rt->assets,"sprint.json");
    if(!weather&&!snapshot_valid)weather = load_preferred(rt->data_dir, rt->assets, "weather.json");
    RsStandings constructor_data;
    bool ok = schedule && drivers && constructors && rs_season_decode_schedule(schedule, &rt->season) &&
              rs_standings_decode_drivers(drivers, &rt->standings) &&
              rs_standings_decode_constructors(constructors, &constructor_data);
    if (ok) {
        rt->standings.constructor_count = constructor_data.constructor_count;
        memcpy(rt->standings.constructors, constructor_data.constructors, sizeof(constructor_data.constructors));
        if (weather) rs_weather_decode(weather, &rt->weather);
        if(results)rs_results_decode(results,RS_RESULT_RACE,&rt->results);
        if(qualifying)rs_results_decode(qualifying,RS_RESULT_QUALIFYING,&rt->results);
        if(sprint)rs_results_decode(sprint,RS_RESULT_SPRINT,&rt->results);
        rs_profiles_rebuild_series(&rt->profiles,&rt->results);
    }
    free(schedule); free(drivers); free(constructors); free(results); free(qualifying); free(sprint); free(weather); cJSON_Delete(snapshot);
    return ok;
}

static void load_favorites(Runtime *rt) {
    char path[1024]; char *text, *line;
    snprintf(path,sizeof(path),"%s/favorites.txt",rt->data_dir); text=rs_store_read(path);
    if(!text)return;
    line=strtok(text,"\r\n");
    while(line){if(!strncmp(line,"driver=",7))snprintf(rt->favorite_driver,sizeof(rt->favorite_driver),"%s",line+7);else if(!strncmp(line,"constructor=",12))snprintf(rt->favorite_constructor,sizeof(rt->favorite_constructor),"%s",line+12);line=strtok(NULL,"\r\n");}
    free(text);
}

static void save_selected_favorite(Runtime *rt) {
    int index=rs_app_cursor(rt->app); char path[1024],contents[160];
    if(rs_app_standings_mode(rt->app)==RS_STANDINGS_CONSTRUCTORS){if(index>=(int)rt->standings.constructor_count)return;snprintf(rt->favorite_constructor,sizeof(rt->favorite_constructor),"%s",rt->standings.constructors[index].id);snprintf(rt->status,sizeof(rt->status),"FAVORITE CONSTRUCTOR SAVED");}
    else {if(index>=(int)rt->standings.driver_count)return;snprintf(rt->favorite_driver,sizeof(rt->favorite_driver),"%s",rt->standings.drivers[index].id);snprintf(rt->status,sizeof(rt->status),"FAVORITE DRIVER SAVED");}
    snprintf(contents,sizeof(contents),"driver=%s\nconstructor=%s\n",rt->favorite_driver,rt->favorite_constructor);
    snprintf(path,sizeof(path),"%s/favorites.txt",rt->data_dir);
    if(!rs_store_write_atomic(path,contents))snprintf(rt->status,sizeof(rt->status),"FAVORITE COULD NOT BE SAVED");
}

static void acknowledge_disclaimer(Runtime *rt) {
    char path[1024];
    snprintf(path,sizeof(path),"%s/acknowledged",rt->data_dir);
    if(rs_store_write_atomic(path,"RaceSlate disclaimer acknowledged\n")){rt->first_launch=false;snprintf(rt->status,sizeof(rt->status),"WELCOME — BASELINE DATA READY");}
    else snprintf(rt->status,sizeof(rt->status),"ACKNOWLEDGEMENT COULD NOT BE SAVED");
}

static char *cached_snapshot_weather(const char *data_dir,RsWeatherSnapshot *weather){char path[1024],*bytes,*json=NULL;cJSON *root;const cJSON *item;snprintf(path,sizeof(path),"%s/snapshot.json",data_dir);bytes=rs_store_read(path);if(!bytes)return NULL;root=cJSON_Parse(bytes);free(bytes);if(!root)return NULL;item=cJSON_GetObjectItemCaseSensitive(root,"weather");if(cJSON_IsObject(item)){json=cJSON_PrintUnformatted(item);if(json&&!rs_weather_decode(json,weather)){free(json);json=NULL;}}cJSON_Delete(root);return json;}

static bool write_device_value(const char *path,const char *value){FILE *file=fopen(path,"w");int written,closed;if(!file)return false;written=fputs(value,file);closed=fclose(file);return written>=0&&closed==0;}
static bool initialize_haptics(void){if(access("/sys/class/gpio/gpio227/value",W_OK)==0)return write_device_value("/sys/class/gpio/gpio227/direction","out")&&write_device_value("/sys/class/gpio/gpio227/value","0");if(!write_device_value("/sys/class/gpio/export","227"))return false;SDL_Delay(50);return write_device_value("/sys/class/gpio/gpio227/direction","out")&&write_device_value("/sys/class/gpio/gpio227/value","0");}
static void pulse_haptic(const Runtime *rt){if(!rt->haptics||!rt->haptic_available)return;if(!write_device_value("/sys/class/gpio/gpio227/value","1"))return;SDL_Delay(24);write_device_value("/sys/class/gpio/gpio227/value","0");}
static void save_settings(Runtime *rt){char path[1024],text[32];snprintf(path,sizeof(path),"%s/settings.txt",rt->data_dir);snprintf(text,sizeof(text),"haptics=%d\n",rt->haptics?1:0);rs_store_write_atomic(path,text);}
static void load_settings(Runtime *rt){char path[1024],*text;rt->haptics=true;snprintf(path,sizeof(path),"%s/settings.txt",rt->data_dir);text=rs_store_read(path);if(text){if(strstr(text,"haptics=0"))rt->haptics=false;free(text);}}
static void perform_settings_action(Runtime *rt){int cursor=rs_app_settings_cursor(rt->app);char path[1024];if(cursor==0){if(!rt->haptic_available){snprintf(rt->status,sizeof(rt->status),"HAPTICS UNSUPPORTED ON THIS DEVICE");return;}rt->haptics=!rt->haptics;save_settings(rt);snprintf(rt->status,sizeof(rt->status),"HAPTICS %s",rt->haptics?"ENABLED":"DISABLED");pulse_haptic(rt);}else if(cursor==1){snprintf(path,sizeof(path),"%s/snapshot.json",rt->data_dir);unlink(path);snprintf(path,sizeof(path),"%s/weather.json",rt->data_dir);unlink(path);snprintf(rt->status,sizeof(rt->status),"DOWNLOADED CACHE CLEARED · BASELINE KEPT");pulse_haptic(rt);}else snprintf(rt->status,sizeof(rt->status),"JOLPICA CC BY-NC-SA · F1DB/OPEN-METEO CC BY · FONTS OFL");}

static int refresh_thread(void *context) {
    RefreshTask *task = context;
    memset(&task->results,0,sizeof(task->results));
    RsHttpResponse schedule = {0}, drivers = {0}, constructors = {0}, results={0}, qualifying={0}, sprint={0},weather_response={0};
    RsSeasonSnapshot season;
    RsStandings driver_data, constructor_data;
    RsWeatherSnapshot weather = {0};
    bool weather_ok = false;
    bool weather_fetched=false;
    bool sprint_valid=false;
    char *cached_weather=NULL;
    bool ok = rs_http_get_https("https://api.jolpi.ca/ergast/f1/current.json?limit=100", task->ca_file, &schedule) &&
              rs_season_decode_schedule(schedule.bytes, &season);
    if (ok) {
        char url[256];
        snprintf(url,sizeof(url),"https://api.jolpi.ca/ergast/f1/%d/driverstandings.json?limit=100",season.season);
        ok=rs_http_get_https(url,task->ca_file,&drivers)&&rs_standings_decode_drivers(drivers.bytes,&driver_data);
        snprintf(url,sizeof(url),"https://api.jolpi.ca/ergast/f1/%d/constructorstandings.json?limit=100",season.season);
        ok=ok&&rs_http_get_https(url,task->ca_file,&constructors)&&rs_standings_decode_constructors(constructors.bytes,&constructor_data);
    }
    if(ok){char url[256];SDL_Delay(300);snprintf(url,sizeof(url),"https://api.jolpi.ca/ergast/f1/%d/results.json?limit=1000",season.season);ok=rs_http_get_https(url,task->ca_file,&results)&&rs_results_decode(results.bytes,RS_RESULT_RACE,&task->results);SDL_Delay(300);snprintf(url,sizeof(url),"https://api.jolpi.ca/ergast/f1/%d/qualifying.json?limit=1000",season.season);ok=ok&&rs_http_get_https(url,task->ca_file,&qualifying)&&rs_results_decode(qualifying.bytes,RS_RESULT_QUALIFYING,&task->results);SDL_Delay(300);snprintf(url,sizeof(url),"https://api.jolpi.ca/ergast/f1/%d/sprint.json?limit=1000",season.season);sprint_valid=rs_http_get_https(url,task->ca_file,&sprint)&&rs_results_decode(sprint.bytes,RS_RESULT_SPRINT,&task->results);}
    if (ok) {
        const RsSession *next = rs_season_next_session(&season, (int64_t)time(NULL));
        const RsEvent *event = event_for_session(&season, next);
        if (event && next) {
            char url[768];
            snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f&hourly=temperature_2m,precipitation_probability,wind_speed_10m&timezone=UTC&wind_speed_unit=kmh&forecast_days=16",
                     event->latitude, event->longitude);
            if (rs_http_get_https(url, task->ca_file, &weather_response) && rs_weather_decode(weather_response.bytes, &weather)) {
                weather_ok=true;weather_fetched=true;
            }
        }
    }
    if(ok&&!weather_fetched){cached_weather=cached_snapshot_weather(task->data_dir,&weather);weather_ok=cached_weather!=NULL;}
    if (ok) {
        char path[1024]; const char *sprint_json=sprint_valid?sprint.bytes:"{\"MRData\":{\"RaceTable\":{\"Races\":[]}}}";const char *weather_json=weather_fetched?weather_response.bytes:cached_weather?cached_weather:"null"; size_t length=schedule.length+drivers.length+constructors.length+results.length+qualifying.length+strlen(sprint_json)+strlen(weather_json)+192; char *generation=malloc(length);
        if(!generation)ok=false;
        else{snprintf(generation,length,"{\"schedule\":%s,\"drivers\":%s,\"constructors\":%s,\"results\":%s,\"qualifying\":%s,\"sprint\":%s,\"weather\":%s}",schedule.bytes,drivers.bytes,constructors.bytes,results.bytes,qualifying.bytes,sprint_json,weather_json);snprintf(path,sizeof(path),"%s/snapshot.json",task->data_dir);ok=rs_store_write_atomic(path,generation);free(generation);}
    }
    SDL_LockMutex(task->mutex);
    if (ok) {
        task->season = season;
        task->standings = driver_data;
        task->standings.constructor_count = constructor_data.constructor_count;
        task->weather = weather;
        task->weather_live=weather_fetched;
        memcpy(task->standings.constructors, constructor_data.constructors, sizeof(constructor_data.constructors));
        snprintf(task->status, sizeof(task->status), weather_fetched ? "ONLINE DATA AND WEATHER UPDATED" : weather_ok?"RACE DATA UPDATED · CACHED WEATHER KEPT":"RACE DATA UPDATED · WEATHER UNAVAILABLE");
    } else snprintf(task->status, sizeof(task->status), "REFRESH FAILED — USING LAST COMPLETE SNAPSHOT");
    task->success = ok;
    task->ready = 1;
    task->running = 0;
    SDL_UnlockMutex(task->mutex);
    free(cached_weather);rs_http_response_dispose(&schedule); rs_http_response_dispose(&drivers); rs_http_response_dispose(&constructors);rs_http_response_dispose(&results);rs_http_response_dispose(&qualifying);rs_http_response_dispose(&sprint);rs_http_response_dispose(&weather_response);
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
    const char *screen=NULL,*detail=NULL; int selected=0;
    int offline = 0, i;
    snprintf(rt.assets, sizeof(rt.assets), "assets");
    snprintf(rt.data_dir, sizeof(rt.data_dir), "data");
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--assets") && i + 1 < argc) snprintf(rt.assets, sizeof(rt.assets), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(rt.data_dir, sizeof(rt.data_dir), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--now") && i + 1 < argc) rt.now_override = strtoll(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!strcmp(argv[i],"--screen")&&i+1<argc)screen=argv[++i];
        else if (!strcmp(argv[i],"--detail")&&i+1<argc)detail=argv[++i];
        else if (!strcmp(argv[i],"--select")&&i+1<argc)selected=atoi(argv[++i]);
        else if (!strcmp(argv[i], "--offline")) offline = 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0 || TTF_Init() != 0 || curl_global_init(CURL_GLOBAL_DEFAULT) != 0) return 1;
    rt.window = SDL_CreateWindow("RaceSlate", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    rt.renderer = SDL_CreateRenderer(rt.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!rt.renderer) rt.renderer = SDL_CreateRenderer(rt.window, -1, SDL_RENDERER_SOFTWARE);
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/fonts/BarlowCondensed-SemiBold.ttf", rt.assets);
        rt.display = TTF_OpenFont(path, 86); rt.heading = TTF_OpenFont(path, 42); rt.small = TTF_OpenFont(path, 17);rt.metric=TTF_OpenFont(path,28);
        snprintf(path, sizeof(path), "%s/fonts/Inter.ttf", rt.assets);
        rt.body = TTF_OpenFont(path, 22);
    }
    rt.app = rs_app_create();
    rt.refresh.mutex = SDL_CreateMutex();
    snprintf(rt.refresh.data_dir, sizeof(rt.refresh.data_dir), "%s", rt.data_dir);
    snprintf(rt.refresh.ca_file, sizeof(rt.refresh.ca_file), "%s/cacert.pem", rt.assets);
    snprintf(rt.status, sizeof(rt.status), "BASELINE DATA · Y TO REFRESH");
    {
        char path[1024]; snprintf(path,sizeof(path),"%s/reference/history.tsv",rt.assets);
        if (!rs_reference_load(path,&rt.reference)) return 2;
        snprintf(path,sizeof(path),"%s/reference/profiles.tsv",rt.assets);
        if (!rs_profiles_load(path,&rt.profiles)) return 2;
    }
    if (!rt.window || !rt.renderer || !rt.display || !rt.heading || !rt.body || !rt.small || !rt.metric || !rt.app || !load_data(&rt)) return 2;
    load_favorites(&rt);
    load_settings(&rt);
    rt.haptic_available=initialize_haptics();
    {char path[1024];char *ack;snprintf(path,sizeof(path),"%s/acknowledged",rt.data_dir);ack=rs_store_read(path);rt.first_launch=ack==NULL&&!screenshot;free(ack);if(rt.first_launch)rs_app_show_disclaimer(rt.app);}
    if (offline) snprintf(rt.status,sizeof(rt.status),"OFFLINE BASELINE DATA");
    if(screen){if(!strcmp(screen,"calendar"))rs_app_dispatch(rt.app,RS_ACTION_R1);else if(!strcmp(screen,"standings")){rs_app_dispatch(rt.app,RS_ACTION_R1);rs_app_dispatch(rt.app,RS_ACTION_R1);}}
    while(selected-->0)rs_app_dispatch(rt.app,RS_ACTION_DOWN);
    if(detail){int cycles=!strcmp(detail,"race")?1:!strcmp(detail,"qualifying")?2:!strcmp(detail,"sprint")?3:0;rs_app_dispatch(rt.app,RS_ACTION_A);while(cycles-->0)rs_app_dispatch(rt.app,RS_ACTION_X);}
    render(&rt);
    if (screenshot) { SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(rt.renderer, NULL, SDL_PIXELFORMAT_ARGB8888, shot->pixels, shot->pitch); SDL_SaveBMP(shot, screenshot); SDL_FreeSurface(shot); rs_app_dispatch(rt.app, RS_ACTION_MENU); }
    while (rs_app_running(rt.app)) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) rs_app_dispatch(rt.app, RS_ACTION_MENU);
            else { RsAction action = map_event(&event); if (action != RS_ACTION_NONE) rs_app_dispatch(rt.app, action); }
        }
        if (rs_app_take_refresh_request(rt.app)) start_refresh(&rt);
        if (rs_app_take_favorite_request(rt.app)) save_selected_favorite(&rt);
        if (rs_app_take_acknowledgement_request(rt.app) && rt.first_launch) acknowledge_disclaimer(&rt);
        if(rs_app_take_settings_action(rt.app))perform_settings_action(&rt);
        if (rt.first_launch && rs_app_overlay(rt.app) == RS_OVERLAY_NONE) rs_app_show_disclaimer(rt.app);
        SDL_LockMutex(rt.refresh.mutex);
        if (rt.refresh.ready) { if (rt.refresh.success) { rt.season = rt.refresh.season; rt.standings = rt.refresh.standings; if(rt.refresh.weather.count)rt.weather = rt.refresh.weather;rt.weather_live=rt.refresh.weather_live!=0; rt.results=rt.refresh.results;rs_profiles_rebuild_series(&rt.profiles,&rt.results); }
            snprintf(rt.status, sizeof(rt.status), "%s", rt.refresh.status); rt.refresh.ready = 0; if(rt.refresh.thread){SDL_DetachThread(rt.refresh.thread);rt.refresh.thread=NULL;} }
        SDL_UnlockMutex(rt.refresh.mutex);
        render(&rt);
        SDL_Delay(16);
    }
    if(rt.refresh.thread) SDL_WaitThread(rt.refresh.thread,NULL);
    TTF_CloseFont(rt.display); TTF_CloseFont(rt.heading); TTF_CloseFont(rt.body); TTF_CloseFont(rt.small);TTF_CloseFont(rt.metric);
    SDL_DestroyMutex(rt.refresh.mutex); rs_app_destroy(rt.app); SDL_DestroyRenderer(rt.renderer); SDL_DestroyWindow(rt.window);
    curl_global_cleanup(); TTF_Quit(); SDL_Quit();
    return 0;
}
