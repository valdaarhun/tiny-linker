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
