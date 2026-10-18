#include "sms_cp1251.h"
#include "sms_path.h"
unsigned short decode_byte(unsigned char value) { return sms_cp1251_decode(value); }
unsigned char encode_word(unsigned short value) { return sms_cp1251_encode(value); }
int encode_path(char *out, unsigned int capacity, const char *in, int cp) { return sms_cp1251_to_utf16le(out, capacity, in, cp); }
int decode_path(char *out, unsigned int capacity, const char *in, unsigned int length, int cp) { return sms_utf16le_to_cp1251(out, capacity, in, length, cp); }
int join_path(char *out, unsigned int capacity, const char *directory, const char *path) { return sms_join_path(out, capacity, directory, path); }
