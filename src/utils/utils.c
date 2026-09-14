#include <stdint.h>
#include <stdbool.h>
#include <string.h>

union byte {
    uint16_t u16;
    uint8_t u8[2];
};

static inline bool is_little_endian() {
    union byte b = {
        .u16 = 1
    };

    return (b.u8[0] == 1);
}

/*
 * Assumption: The object file and the machine on which the files
 * are to be linked and executed will be the same.
 */

void read_u8(void *result, uint8_t *buffer, uint8_t *offset) {
    memcpy(result, (void *)buffer, 1);
    if (offset) *offset += 1;
}

void read_u16(void *result, uint8_t *buffer, uint8_t *offset) {
    uint16_t u16 = 0;
    if (is_little_endian()) {
        u16 = buffer[1];
        u16 = (u16 << 8) | buffer[0];
    } else {
        u16 = buffer[0];
        u16 = (u16 << 8) | buffer[1];
    }
    memcpy(result, (void *)&u16, 2);
    if (offset) *offset += 2;
}

void read_u32(void *result, uint8_t *buffer, uint8_t *offset) {
    uint32_t u32 = 0;
    if (is_little_endian()) {
        u32 = buffer[3];
        u32 = (u32 << 8) | buffer[2];
        u32 = (u32 << 8) | buffer[1];
        u32 = (u32 << 8) | buffer[0];
    } else {
        u32 = buffer[0];
        u32 = (u32 << 8) | buffer[1];
        u32 = (u32 << 8) | buffer[2];
        u32 = (u32 << 8) | buffer[3];
    }
    memcpy(result, (void *)&u32, 4);
    if (offset) *offset += 4;
}

void read_u64(void *result, uint8_t *buffer, uint8_t *offset) {
    uint64_t u64 = 0;
    if (is_little_endian()) {
        u64 = buffer[7];
        u64 = (u64 << 8) | buffer[6];
        u64 = (u64 << 8) | buffer[5];
        u64 = (u64 << 8) | buffer[4];
        u64 = (u64 << 8) | buffer[3];
        u64 = (u64 << 8) | buffer[2];
        u64 = (u64 << 8) | buffer[1];
        u64 = (u64 << 8) | buffer[0];
    } else {
        u64 = buffer[0];
        u64 = (u64 << 8) | buffer[1];
        u64 = (u64 << 8) | buffer[2];
        u64 = (u64 << 8) | buffer[3];
        u64 = (u64 << 8) | buffer[4];
        u64 = (u64 << 8) | buffer[5];
        u64 = (u64 << 8) | buffer[6];
        u64 = (u64 << 8) | buffer[7];
    }
    memcpy(result, (void *)&u64, 8);
    if (offset) *offset += 8;
}

void write_u8(uint8_t u8, uint8_t *buffer, uint8_t *offset) {
    buffer[*offset] = u8;
    *offset += 1;
}

void write_u16(uint16_t u16, uint8_t *buffer, uint8_t *offset) {
    uint8_t ptr = *offset;
    if (is_little_endian()) {
        buffer[ptr] = u16 & 0xff;
        buffer[ptr + 1] = (u16 >> 8) & 0xff;
    } else {
        buffer[ptr + 1] = u16 & 0xff;
        buffer[ptr] = (u16 >> 8) & 0xff;
    }
    *offset += 2;
}

void write_u32(uint32_t u32, uint8_t *buffer, uint8_t *offset) {
    uint8_t ptr = *offset;
    if (is_little_endian()) {
        buffer[ptr] = u32 & 0xff;
        buffer[ptr + 1] = (u32 >> 8) & 0xff;
        buffer[ptr + 2] = (u32 >> 16) & 0xff;
        buffer[ptr + 3] = (u32 >> 24) & 0xff;
    } else {
        buffer[ptr + 3] = u32 & 0xff;
        buffer[ptr + 2] = (u32 >> 8) & 0xff;
        buffer[ptr + 1] = (u32 >> 16) & 0xff;
        buffer[ptr] = (u32 >> 24) & 0xff;
    }
    *offset += 4;
}

void write_u64(uint64_t u64, uint8_t *buffer, uint8_t *offset) {
    uint8_t ptr = *offset;
    if (is_little_endian()) {
        buffer[ptr] = u64 & 0xff;
        buffer[ptr + 1] = (u64 >> 8) & 0xff;
        buffer[ptr + 2] = (u64 >> 16) & 0xff;
        buffer[ptr + 3] = (u64 >> 24) & 0xff;
        buffer[ptr + 4] = (u64 >> 32) & 0xff;
        buffer[ptr + 5] = (u64 >> 40) & 0xff;
        buffer[ptr + 6] = (u64 >> 48) & 0xff;
        buffer[ptr + 7] = (u64 >> 56) & 0xff;
    } else {
        buffer[ptr + 7] = u64 & 0xff;
        buffer[ptr + 6] = (u64 >> 8) & 0xff;
        buffer[ptr + 5] = (u64 >> 16) & 0xff;
        buffer[ptr + 4] = (u64 >> 24) & 0xff;
        buffer[ptr + 3] = (u64 >> 32) & 0xff;
        buffer[ptr + 2] = (u64 >> 40) & 0xff;
        buffer[ptr + 1] = (u64 >> 48) & 0xff;
        buffer[ptr] = (u64 >> 56) & 0xff;
    }
    *offset += 8;
}
