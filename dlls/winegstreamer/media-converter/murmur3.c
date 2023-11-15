/*
 * Copyright 2024 Ziqing Hui for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "media-converter.h"

static uint64_t rotl64(uint64_t value, uint8_t shift)
{
    return (value << shift) | (value >> (64 - shift));
}

static uint32_t rotl32(uint32_t value, uint8_t shift)
{
    return (value << shift) | (value >> (32 - shift));
}

static uint64_t fmix64(uint64_t h)
{
    const uint64_t c1 = 0xff51afd7ed558ccd;
    const uint64_t c2 = 0xc4ceb9fe1a85ec53;
    const uint32_t r = 33;

    h ^= h >> r;
    h *= c1;
    h ^= h >> r;
    h *= c2;
    h ^= h >> r;

    return h;
}

static uint32_t fmix32 (uint32_t h)
{
    const uint32_t c1 = 0x85ebca6b;
    const uint32_t c2 = 0xc2b2ae35;
    const uint32_t r1 = 16;
    const uint32_t r2 = 13;

    h ^= h >> r1;
    h *= c1;
    h ^= h >> r2;
    h *= c2;
    h ^= h >> r1;

    return h;
}

void murmur3_x64_128_state_init(struct murmur3_x64_128_state *state, uint32_t seed)
{
    state->seed = seed;
    state->h1 = seed;
    state->h2 = seed;
    state->processed = 0;
}

void murmur3_x64_128_state_reset(struct murmur3_x64_128_state *state)
{
    state->h1 = state->seed;
    state->h2 = state->seed;
    state->processed = 0;
}

bool murmur3_x64_128_full(void *data_reader, data_read_callback read_callback,
        struct murmur3_x64_128_state* state, void *out)
{
    const uint64_t c1 = 0x87c37b91114253d5, c2 = 0x4cf5ad432745937f, c3 = 0x52dce729, c4 = 0x38495ab5, m = 5;
    size_t read_size, processed = state->processed;
    const uint32_t r1 = 27, r2 = 31, r3 = 33;
    uint64_t h1 = state->h1, h2 = state->h2;
    uint8_t buffer[16] = {0};
    uint64_t k1, k2;
    int ret;

    while ((ret = read_callback(data_reader, buffer, sizeof(buffer), &read_size)) == CONV_OK)
    {
        processed += read_size;

        if (read_size == 16)
        {
            k1 = *(uint64_t *)&buffer[0];
            k2 = *(uint64_t *)&buffer[8];

            k1 *= c1;
            k1 = rotl64(k1, r2);
            k1 *= c2;
            h1 ^= k1;
            h1 = rotl64(h1, r1);
            h1 += h2;
            h1 = h1 * m + c3;

            k2 *= c2;
            k2 = rotl64(k2, r3);
            k2 *= c1;
            h2 ^= k2;
            h2 = rotl64(h2, r2);
            h2 += h1;
            h2 = h2 * m + c4;
        }
        else
        {
            k1 = 0;
            k2 = 0;

            switch (read_size)
            {
            case 15:
                k2 ^= ((uint64_t)buffer[14]) << 48;
            case 14:
                k2 ^= ((uint64_t)buffer[13]) << 40;
            case 13:
                k2 ^= ((uint64_t)buffer[12]) << 32;
            case 12:
                k2 ^= ((uint64_t)buffer[11]) << 24;
            case 11:
                k2 ^= ((uint64_t)buffer[10]) << 16;
            case 10:
                k2 ^= ((uint64_t)buffer[9]) << 8;
            case 9:
                k2 ^= ((uint64_t)buffer[8]) << 0;
                k2 *= c2;
                k2  = rotl64(k2, r3);
                k2 *= c1;
                h2 ^= k2;
            case 8:
                k1 ^= ((uint64_t)buffer[7]) << 56;
            case 7:
                k1 ^= ((uint64_t)buffer[6]) << 48;
            case 6:
                k1 ^= ((uint64_t)buffer[5]) << 40;
            case 5:
                k1 ^= ((uint64_t)buffer[4]) << 32;
            case 4:
                k1 ^= ((uint64_t)buffer[3]) << 24;
            case 3:
                k1 ^= ((uint64_t)buffer[2]) << 16;
            case 2:
                k1 ^= ((uint64_t)buffer[1]) << 8;
            case 1:
                k1 ^= ((uint64_t)buffer[0]) << 0;
                k1 *= c1;
                k1  = rotl64(k1, r2);
                k1 *= c2;
                h1 ^= k1;
            }
        }
    }

    if (ret != CONV_ERROR_DATA_END)
        return false;

    state->processed = processed;
    state->h1 = h1;
    state->h2 = h2;

    h1 ^= (uint64_t)processed;
    h2 ^= (uint64_t)processed;
    h1 += h2;
    h2 += h1;
    h1 = fmix64(h1);
    h2 = fmix64(h2);
    h1 += h2;
    h2 += h1;

    ((uint64_t *)out)[0] = h1;
    ((uint64_t *)out)[1] = h2;

    return true;
}

bool murmur3_x64_128(void *data_src, data_read_callback read_callback, uint32_t seed, void *out)
{
    struct murmur3_x64_128_state state;
    murmur3_x64_128_state_init(&state, seed);
    return murmur3_x64_128_full(data_src, read_callback, &state, out);
}

void murmur3_x86_128_state_init(struct murmur3_x86_128_state *state, uint32_t seed)
{
    state->seed = seed;
    state->h1 = seed;
    state->h2 = seed;
    state->h3 = seed;
    state->h4 = seed;
    state->processed = 0;
}

void murmur3_x86_128_state_reset(struct murmur3_x86_128_state *state)
{
    state->h1 = state->seed;
    state->h2 = state->seed;
    state->h3 = state->seed;
    state->h4 = state->seed;
    state->processed = 0;
}

bool murmur3_x86_128_full(void *data_reader, data_read_callback read_callback,
        struct murmur3_x86_128_state *state, void *out)
{
    const uint32_t c1 = 0x239b961b, c2 = 0xab0e9789, c3 = 0x38b34ae5, c4 = 0xa1e38b93;
    const uint32_t c5 = 0x561ccd1b, c6 = 0x0bcaa747, c7 = 0x96cd1c35, c8 = 0x32ac3b17;
    uint32_t h1 = state->h1, h2 = state->h2, h3 = state->h3, h4 = state->h4;
    size_t read_size, processed = state->processed;
    unsigned char buffer[16] = {0};
    uint64_t k1, k2, k3, k4;
    const uint32_t m = 5;
    int ret;

    while ((ret = read_callback(data_reader, buffer, sizeof(buffer), &read_size)) == CONV_OK)
    {
        processed += read_size;

        if (read_size == 16)
        {
            k1 = *(uint32_t*)&buffer[0];
            k2 = *(uint32_t*)&buffer[4];
            k3 = *(uint32_t*)&buffer[8];
            k4 = *(uint32_t*)&buffer[12];

            k1 *= c1;
            k1 = rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
            h1 = rotl32(h1, 19);
            h1 += h2;
            h1 = h1 * m + c5;

            k2 *= c2;
            k2 = rotl32(k2, 16);
            k2 *= c3;
            h2 ^= k2;
            h2 = rotl32(h2, 17);
            h2 += h3;
            h2 = h2 * m + c6;

            k3 *= c3;
            k3 = rotl32(k3, 17);
            k3 *= c4;
            h3 ^= k3;
            h3 = rotl32(h3, 15);
            h3 += h4;
            h3 = h3 * m + c7;

            k4 *= c4;
            k4 = rotl32(k4, 18);
            k4 *= c1;
            h4 ^= k4;
            h4 = rotl32(h4, 13);
            h4 += h1;
            h4 = h4 * m + c8;
        }
        else
        {
            k1 = 0;
            k2 = 0;
            k3 = 0;
            k4 = 0;

            switch (read_size)
            {
            case 15:
                k4 ^= buffer[14] << 16;
            case 14:
                k4 ^= buffer[13] << 8;
            case 13:
                k4 ^= buffer[12] << 0;
                k4 *= c4;
                k4 = rotl32(k4,18);
                k4 *= c1;
                h4 ^= k4;
            case 12:
                k3 ^= buffer[11] << 24;
            case 11:
                k3 ^= buffer[10] << 16;
            case 10:
                k3 ^= buffer[9] << 8;
            case 9:
                k3 ^= buffer[8] << 0;
                k3 *= c3;
                k3 = rotl32(k3, 17);
                k3 *= c4;
                h3 ^= k3;
            case 8:
                k2 ^= buffer[7] << 24;
            case 7:
                k2 ^= buffer[6] << 16;
            case 6:
                k2 ^= buffer[5] << 8;
            case 5:
                k2 ^= buffer[4] << 0;
                k2 *= c2;
                k2 = rotl32(k2, 16);
                k2 *= c3;
                h2 ^= k2;
            case 4:
                k1 ^= buffer[3] << 24;
            case 3:
                k1 ^= buffer[2] << 16;
            case 2:
                k1 ^= buffer[1] << 8;
            case 1:
                k1 ^= buffer[0] << 0;
                k1 *= c1;
                k1 = rotl32(k1, 15);
                k1 *= c2;
                h1 ^= k1;
            }
        }
    }

    if (ret != CONV_ERROR_DATA_END)
        return false;

    state->processed = processed;
    state->h1 = h1;
    state->h2 = h2;
    state->h3 = h3;
    state->h4 = h4;

    h1 ^= processed;
    h2 ^= processed;
    h3 ^= processed;
    h4 ^= processed;
    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;
    h1 = fmix32(h1);
    h2 = fmix32(h2);
    h3 = fmix32(h3);
    h4 = fmix32(h4);
    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;

    ((uint32_t*)out)[0] = h1;
    ((uint32_t*)out)[1] = h2;
    ((uint32_t*)out)[2] = h3;
    ((uint32_t*)out)[3] = h4;

    return true;
}

bool murmur3_x86_128(void *data_src, data_read_callback read_callback, uint32_t seed, void *out)
{
    struct murmur3_x86_128_state state;
    murmur3_x86_128_state_init(&state, seed);
    return murmur3_x86_128_full(data_src, read_callback, &state, out);
}
