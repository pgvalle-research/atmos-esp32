#ifndef IR_DECODER_H
#define IR_DECODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    IR_PROTOCOL_UNKNOWN = 0,
    IR_PROTOCOL_SONY,
    IR_PROTOCOL_SAMSUNG,
} ir_protocol_t;

typedef struct {
    uint16_t duration_us;
    uint8_t level;
} ir_pulse_t;

typedef struct {
    ir_protocol_t protocol;
    uint32_t address;
    uint32_t command;
    uint64_t raw;
    uint8_t bits;
    bool repeat;
} ir_frame_t;

bool ir_decode(const ir_pulse_t *pulses, size_t count, ir_frame_t *frame);
const char *ir_protocol_name(ir_protocol_t protocol);

#endif
