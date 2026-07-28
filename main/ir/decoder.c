#include "decoder.h"

#include <string.h>

#define TOLERANCE_PERCENT 30U

static bool near_us(uint32_t actual, uint32_t expected)
{
    uint32_t margin = (expected * TOLERANCE_PERCENT) / 100U;
    return actual >= expected - margin && actual <= expected + margin;
}

static size_t find_header(const ir_pulse_t *pulses, size_t count,
                          uint32_t mark_us, uint32_t space_us)
{
    for (size_t i = 0; i + 1 < count; ++i) {
        if (pulses[i].level == 0 && pulses[i + 1].level == 1 &&
            near_us(pulses[i].duration_us, mark_us) &&
            near_us(pulses[i + 1].duration_us, space_us)) {
            return i;
        }
    }
    return count;
}

static bool decode_samsung(const ir_pulse_t *pulses, size_t count,
                           ir_frame_t *frame)
{
    size_t first = find_header(pulses, count, 4500, 4500);
    if (first == count) {
        /* Samsung's repeat frame uses a 4.5 ms mark and 2.25 ms space. */
        first = find_header(pulses, count, 4500, 2250);
        if (first != count && first + 2 < count &&
            pulses[first + 2].level == 0 &&
            near_us(pulses[first + 2].duration_us, 560)) {
            frame->protocol = IR_PROTOCOL_SAMSUNG;
            frame->repeat = true;
            return true;
        }
        return false;
    }

    size_t pos = first + 2;
    uint32_t raw = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        if (pos + 1 >= count ||
            pulses[pos].level != 0 || pulses[pos + 1].level != 1 ||
            !near_us(pulses[pos].duration_us, 560)) {
            return false;
        }

        if (near_us(pulses[pos + 1].duration_us, 1690)) {
            raw |= 1UL << bit;
        } else if (!near_us(pulses[pos + 1].duration_us, 560)) {
            return false;
        }
        pos += 2;
    }

    frame->protocol = IR_PROTOCOL_SAMSUNG;
    frame->address = raw & 0xffffU;
    frame->command = (raw >> 16) & 0xffU;
    frame->raw = raw;
    frame->bits = 32;
    return true;
}

static bool decode_sony(const ir_pulse_t *pulses, size_t count,
                        ir_frame_t *frame)
{
    size_t first = find_header(pulses, count, 2400, 600);
    if (first == count) {
        return false;
    }

    size_t pos = first + 2;
    uint64_t raw = 0;
    unsigned bits = 0;
    while (bits < 20 && pos < count) {
        if (pulses[pos].level != 0) {
            break;
        }

        if (near_us(pulses[pos].duration_us, 1200)) {
            raw |= 1ULL << bits;
        } else if (!near_us(pulses[pos].duration_us, 600)) {
            break;
        }

        ++bits;
        ++pos;
        if (pos < count && pulses[pos].level == 1 &&
            near_us(pulses[pos].duration_us, 600)) {
            ++pos;
        } else if (bits != 12 && bits != 15 && bits != 20) {
            return false;
        }
    }

    if (bits != 12 && bits != 15 && bits != 20) {
        return false;
    }

    frame->protocol = IR_PROTOCOL_SONY;
    frame->command = raw & 0x7fU;
    frame->address = (uint32_t)(raw >> 7);
    frame->raw = raw;
    frame->bits = (uint8_t)bits;
    return true;
}

bool ir_decode(const ir_pulse_t *pulses, size_t count, ir_frame_t *frame)
{
    if (pulses == NULL || frame == NULL || count < 3) {
        return false;
    }

    memset(frame, 0, sizeof(*frame));
    return decode_samsung(pulses, count, frame) ||
           decode_sony(pulses, count, frame);
}

const char *ir_protocol_name(ir_protocol_t protocol)
{
    switch (protocol) {
    case IR_PROTOCOL_SONY:
        return "sony";
    case IR_PROTOCOL_SAMSUNG:
        return "samsung";
    default:
        return "unknown";
    }
}
