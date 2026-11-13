#ifndef QUARK_DE_WORKSPACE_H
#define QUARK_DE_WORKSPACE_H

#include <quark/de.h>
#include <quark/de_apps.h>

#define DE_WORKSPACE_TERMINAL 0
#define DE_WORKSPACE_DESKTOP  1
#define DE_WORKSPACE_COUNT    2

extern volatile int g_workspace;

void de_workspace_init(void);
void de_workspace_switch(int ws);
void de_workspace_next(void);
void de_workspace_prev(void);
int  de_workspace_handle_key(const de_key_event_t* k);

void ws_terminal_init(void);
void ws_terminal_draw(void);
int  ws_terminal_handle_key(const de_key_event_t* k);
void ws_terminal_log_line(const char* line);

#endif