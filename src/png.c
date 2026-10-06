#include "png.h"
#include <stdio.h>
#include <stdlib.h>

/* CRC32 (required by the PNG format) */
static uint32_t crc_table[256];
static int crc_ready = 0;

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++)
    {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
        {
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        crc_table[n] = c;
    }
    crc_ready = 1;
}

static uint32_t crc_update(uint32_t crc, const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[4];
    put32(hdr, len);
    fwrite(hdr, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (len)
        fwrite(data, 1, len, f);

    uint32_t crc = 0xFFFFFFFFu;
    crc = crc_update(crc, (const uint8_t *)type, 4);
    crc = crc_update(crc, data, len);
    put32(hdr, crc ^ 0xFFFFFFFFu);
    fwrite(hdr, 1, 4, f);
}

int png_save(const char *path, const uint32_t *pixels, int w, int h, int scale)
{
    if (!crc_ready)
        crc_init();

    int ow = w * scale, oh = h * scale;
    size_t row_bytes = 1 + (size_t)ow * 3; /* 1 filter byte + RGB */
    size_t raw_len = row_bytes * (size_t)oh;
    uint8_t *raw = malloc(raw_len);
    if (!raw)
        return 0;

    for (int y = 0; y < oh; y++)
    {
        uint8_t *row = raw + (size_t)y * row_bytes;
        *row++ = 0;
        for (int x = 0; x < ow; x++)
        {
            uint32_t px = pixels[(y / scale) * w + (x / scale)];
            *row++ = (uint8_t)(px >> 16);
            *row++ = (uint8_t)(px >> 8);
            *row++ = (uint8_t)px;
        }
    }

    /* zlib without compression: "stored" blocks of up to 65535 bytes */
    size_t blocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + blocks * 5 + 4;
    uint8_t *z = malloc(z_len);
    if (!z)
    {
        free(raw);
        return 0;
    }

    size_t zi = 0;
    z[zi++] = 0x78;
    z[zi++] = 0x01;
    uint32_t a = 1, b = 0; /* Adler-32 */
    for (size_t off = 0; off < raw_len;)
    {
        size_t chunk = raw_len - off > 65535 ? 65535 : raw_len - off;
        z[zi++] = (off + chunk == raw_len) ? 1 : 0;
        z[zi++] = (uint8_t)(chunk & 0xFF);
        z[zi++] = (uint8_t)(chunk >> 8);
        z[zi++] = (uint8_t)(~chunk & 0xFF);
        z[zi++] = (uint8_t)((~chunk >> 8) & 0xFF);
        for (size_t i = 0; i < chunk; i++)
        {
            uint8_t byte = raw[off + i];
            z[zi++] = byte;
            a = (a + byte) % 65521;
            b = (b + a) % 65521;
        }
        off += chunk;
    }
    put32(z + zi, (b << 16) | a);
    zi += 4;

    FILE *f = fopen(path, "wb");
    if (!f)
    {
        free(raw);
        free(z);
        return 0;
    }

    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, f);

    uint8_t ihdr[13];
    put32(ihdr, (uint32_t)ow);
    put32(ihdr + 4, (uint32_t)oh);
    ihdr[8] = 8; /* 8 bits per channel */
    ihdr[9] = 2; /* RGB */
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    write_chunk(f, "IHDR", ihdr, 13);
    write_chunk(f, "IDAT", z, (uint32_t)zi);
    write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(raw);
    free(z);
    return 1;
}