#pragma once

#include <3ds/types.h>
#include "menu.h"
#include "menus.h"
#include "sock_util.h"
#include "utils.h"

extern Menu streamingMenu;

void startMainThread(void);
void endThread(void);
void finalize(void);
void rpCloseGameHandle(void);
void closeRPHandle(void);
