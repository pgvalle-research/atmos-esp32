#ifndef BINDINGS_H
#define BINDINGS_H

#include "lua.h"

void register_gpio(lua_State *L);
void register_espnow(lua_State *L);

#endif // ESP_BINDINGS_H
