#ifndef RS_APP_H
#define RS_APP_H

#include <stdbool.h>

typedef struct RsApp RsApp;

typedef enum { RS_ROUTE_NEXT, RS_ROUTE_CALENDAR, RS_ROUTE_STANDINGS } RsRoute;
typedef enum { RS_OVERLAY_NONE, RS_OVERLAY_ABOUT } RsOverlay;
typedef enum { RS_STANDINGS_DRIVERS, RS_STANDINGS_CONSTRUCTORS } RsStandingsMode;
typedef enum {
    RS_ACTION_NONE, RS_ACTION_UP, RS_ACTION_DOWN, RS_ACTION_LEFT, RS_ACTION_RIGHT,
    RS_ACTION_A, RS_ACTION_B, RS_ACTION_X, RS_ACTION_Y, RS_ACTION_L1, RS_ACTION_R1,
    RS_ACTION_L2, RS_ACTION_R2, RS_ACTION_SELECT, RS_ACTION_START, RS_ACTION_MENU
} RsAction;

RsApp *rs_app_create(void);
void rs_app_destroy(RsApp *app);
void rs_app_dispatch(RsApp *app, RsAction action);
RsRoute rs_app_route(const RsApp *app);
RsOverlay rs_app_overlay(const RsApp *app);
RsStandingsMode rs_app_standings_mode(const RsApp *app);
bool rs_app_running(const RsApp *app);
int rs_app_cursor(const RsApp *app);
bool rs_app_take_refresh_request(RsApp *app);

#endif
