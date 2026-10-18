/* SMB wire UTF-16LE <-> SMS WinCyrillic (Windows-1251).
 * ASCII stays byte-identical. No UI, font or UDPFS changes are required.
 * AFL-3.0. Strict CP1251 mode rejects lossy names (not wildcard '?').
 */
#ifndef SMS_CP1251_H
#define SMS_CP1251_H

static const unsigned short sms_cp1251_80_bf[64] = {
    0x0402, 0x0403, 0x201a, 0x0453, 0x201e, 0x2026, 0x2020, 0x2021,
    0x20ac, 0x2030, 0x0409, 0x2039, 0x040a, 0x040c, 0x040b, 0x040f,
    0x0452, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0xffff, 0x2122, 0x0459, 0x203a, 0x045a, 0x045c, 0x045b, 0x045f,
    0x00a0, 0x040e, 0x045e, 0x0408, 0x00a4, 0x0490, 0x00a6, 0x00a7,
    0x0401, 0x00a9, 0x0404, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x0407,
    0x00b0, 0x00b1, 0x0406, 0x0456, 0x0491, 0x00b5, 0x00b6, 0x00b7,
    0x0451, 0x2116, 0x0454, 0x00bb, 0x0458, 0x0405, 0x0455, 0x0457
};

static unsigned short sms_cp1251_decode(unsigned char value)
{
    if (value < 0x80)
        return value;
    if (value >= 0xc0)
        return (unsigned short)(0x0410 + value - 0xc0);
    if (value == 0x98)
        return '?';
    return sms_cp1251_80_bf[value - 0x80];
}

static unsigned char sms_cp1251_encode(unsigned short value)
{
    unsigned int i;
    if (value < 0x80)
        return (unsigned char)value;
    if (value >= 0x0410 && value <= 0x044f)
        return (unsigned char)(0xc0 + value - 0x0410);
    for (i = 0; i < 64; ++i)
        if (sms_cp1251_80_bf[i] != 0xffff && sms_cp1251_80_bf[i] == value)
            return (unsigned char)(i + 0x80);
    return '?';
}

static int sms_cp1251_to_utf16le(char *out, unsigned int capacity,
                                const char *in, int cp1251)
{
    const unsigned char *source = (const unsigned char *)in;
    unsigned char *destination = (unsigned char *)out;
    int length = 0;
    if (capacity < 2)
        return -1;
    out[0] = out[1] = 0;
    while (*source) {
        unsigned short codepoint;
        if ((unsigned int)length + 4 > capacity || (cp1251 && *source == 0x98))
            return -1;
        codepoint = cp1251 ? sms_cp1251_decode(*source++) : *source++;
        *destination++ = (unsigned char)(codepoint & 0xff);
        *destination++ = (unsigned char)(codepoint >> 8);
        length += 2;
        destination[0] = destination[1] = 0;
    }
    *destination++ = 0;
    *destination = 0;
    return length + 2;
}

/* inbytes is always a strict wire-byte bound, including for terminated input.
 * On error return -1; any partial output remains terminated. */
static int sms_utf16le_to_cp1251(char *out, unsigned int capacity,
                                const char *in, unsigned int inbytes, int cp1251)
{
    const unsigned char *source = (const unsigned char *)in;
    unsigned char *destination = (unsigned char *)out;
    unsigned int consumed = 0;
    unsigned int length = 0;
    if (!capacity)
        return -1;
    *out = 0;
    if (inbytes & 1)
        return -1;
    while (consumed + 2 <= inbytes) {
        unsigned short codepoint = (unsigned short)(source[0] | ((unsigned short)source[1] << 8));
        unsigned char encoded;
        if (!codepoint)
            break;
        source += 2;
        consumed += 2;
        if (!cp1251 && codepoint >= 0xd800 && codepoint <= 0xdbff && consumed + 2 <= inbytes) {
            unsigned short low = (unsigned short)(source[0] | ((unsigned short)source[1] << 8));
            if (low >= 0xdc00 && low <= 0xdfff) {
                source += 2;
                consumed += 2;
            }
        }
        if (length + 1 >= capacity)
            return -1;
        encoded = cp1251 ? sms_cp1251_encode(codepoint) :
                          (codepoint <= 128 ? (unsigned char)codepoint : '?');
        if (cp1251 && encoded == '?' && codepoint != '?')
            return -1;
        *destination++ = encoded;
        *destination = 0;
        ++length;
    }
    *destination = 0;
    return length + 1;
}

#endif
