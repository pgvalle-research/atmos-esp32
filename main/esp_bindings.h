#ifndef ESP_BINDINGS_H
#define ESP_BINDINGS_H

#include "lua.h"

void register_pins_esp(lua_State *L);
void register_pwm_esp(lua_State *L);
void register_adc_esp(lua_State *L);

#endif // ESP_BINDINGS_H
