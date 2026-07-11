#include "rs_app.h"

#include <stdlib.h>

struct RsApp {
    RsRoute route;
    RsOverlay overlay;
    RsStandingsMode standings_mode;
    int cursor[3];
    bool running;
    bool refresh_requested;
    bool favorite_requested;
    bool acknowledgement_requested;
    bool settings_action_requested;
    int settings_cursor;
    RsDetailMode detail_mode;
    int detail_cursor;
    bool track_time;
};

RsApp *rs_app_create(void) {
    RsApp *app = calloc(1, sizeof(*app));
    if (app) app->running = true;
    return app;
}

void rs_app_destroy(RsApp *app) { free(app); }

void rs_app_dispatch(RsApp *app, RsAction action) {
    if (!app || !app->running) return;
    if (action == RS_ACTION_MENU) { app->running = false; return; }
    if (app->overlay != RS_OVERLAY_NONE) {
        if(app->overlay==RS_OVERLAY_DISCLAIMER){if(action==RS_ACTION_A){app->acknowledgement_requested=true;app->overlay=RS_OVERLAY_NONE;}return;}
        if (action == RS_ACTION_B || action == RS_ACTION_START) app->overlay = RS_OVERLAY_NONE;
        else if(app->overlay==RS_OVERLAY_ABOUT&&action==RS_ACTION_UP&&app->settings_cursor>0)app->settings_cursor--;
        else if(app->overlay==RS_OVERLAY_ABOUT&&action==RS_ACTION_DOWN&&app->settings_cursor<2)app->settings_cursor++;
        else if(app->overlay==RS_OVERLAY_ABOUT&&action==RS_ACTION_A)app->settings_action_requested=true;
        else if(app->overlay==RS_OVERLAY_DETAIL&&action==RS_ACTION_X){app->detail_mode=app->route==RS_ROUTE_STANDINGS?(app->detail_mode==RS_DETAIL_HISTORY?RS_DETAIL_RACE:RS_DETAIL_HISTORY):(RsDetailMode)((app->detail_mode+1)%4);app->detail_cursor=0;}
        else if(app->overlay==RS_OVERLAY_DETAIL&&action==RS_ACTION_DOWN)app->detail_cursor++;
        else if(app->overlay==RS_OVERLAY_DETAIL&&action==RS_ACTION_UP&&app->detail_cursor>0)app->detail_cursor--;
        else if(app->overlay==RS_OVERLAY_DETAIL&&action==RS_ACTION_SELECT&&app->route==RS_ROUTE_STANDINGS)app->favorite_requested=true;
        return;
    }
    switch (action) {
        case RS_ACTION_L1: app->route = (RsRoute)((app->route + 2) % 3); break;
        case RS_ACTION_R1: app->route = (RsRoute)((app->route + 1) % 3); break;
        case RS_ACTION_UP: if (app->cursor[app->route] > 0) app->cursor[app->route]--; break;
        case RS_ACTION_DOWN: app->cursor[app->route]++; break;
        case RS_ACTION_X:
            if(app->route==RS_ROUTE_NEXT)app->track_time=!app->track_time;
            else if (app->route == RS_ROUTE_STANDINGS)
                app->standings_mode = app->standings_mode == RS_STANDINGS_DRIVERS
                    ? RS_STANDINGS_CONSTRUCTORS : RS_STANDINGS_DRIVERS;
            break;
        case RS_ACTION_Y: app->refresh_requested = true; break;
        case RS_ACTION_A: if (app->route != RS_ROUTE_NEXT) {app->overlay = RS_OVERLAY_DETAIL;app->detail_mode=RS_DETAIL_HISTORY;app->detail_cursor=0;} break;
        case RS_ACTION_L2: if(app->route==RS_ROUTE_CALENDAR&&app->cursor[app->route]>0)app->cursor[app->route]--;break;
        case RS_ACTION_R2: if(app->route==RS_ROUTE_CALENDAR)app->cursor[app->route]++;break;
        case RS_ACTION_START: app->overlay = RS_OVERLAY_ABOUT; break;
        default: break;
    }
}

RsRoute rs_app_route(const RsApp *app) { return app ? app->route : RS_ROUTE_NEXT; }
RsOverlay rs_app_overlay(const RsApp *app) { return app ? app->overlay : RS_OVERLAY_NONE; }
RsStandingsMode rs_app_standings_mode(const RsApp *app) {
    return app ? app->standings_mode : RS_STANDINGS_DRIVERS;
}
bool rs_app_running(const RsApp *app) { return app && app->running; }
int rs_app_cursor(const RsApp *app) { return app ? app->cursor[app->route] : 0; }
bool rs_app_take_refresh_request(RsApp *app) {
    bool requested;
    if (!app) return false;
    requested = app->refresh_requested;
    app->refresh_requested = false;
    return requested;
}
bool rs_app_take_favorite_request(RsApp *app) {
    bool requested;
    if (!app) return false;
    requested = app->favorite_requested;
    app->favorite_requested = false;
    return requested;
}
bool rs_app_take_acknowledgement_request(RsApp *app) {
    bool requested;
    if (!app) return false;
    requested = app->acknowledgement_requested;
    app->acknowledgement_requested = false;
    return requested;
}
RsDetailMode rs_app_detail_mode(const RsApp *app){return app?app->detail_mode:RS_DETAIL_HISTORY;}
int rs_app_detail_cursor(const RsApp *app){return app?app->detail_cursor:0;}
bool rs_app_track_time(const RsApp *app){return app&&app->track_time;}
void rs_app_show_disclaimer(RsApp *app){if(app)app->overlay=RS_OVERLAY_DISCLAIMER;}
int rs_app_settings_cursor(const RsApp *app){return app?app->settings_cursor:0;}
bool rs_app_take_settings_action(RsApp *app){bool value;if(!app)return false;value=app->settings_action_requested;app->settings_action_requested=false;return value;}
