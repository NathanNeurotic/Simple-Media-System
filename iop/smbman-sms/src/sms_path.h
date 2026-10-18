/* AFL-3.0. Bounded legacy SMB path join, without silent truncation. */
#ifndef SMS_PATH_H
#define SMS_PATH_H
static int sms_join_path(char *out, unsigned int capacity,
                         const char *directory, const char *path)
{
    unsigned int i = 0, start = 0, end = 0, n = 0;
    while (directory[i]) ++i;
    while (path[end]) ++end;
    while (start < end && (path[start] == '/' || path[start] == '\\')) ++start;
    while (end > start && (path[end - 1] == '/' || path[end - 1] == '\\')) --end;
    if (!capacity || i >= capacity || end - start >= capacity - i - 1)
        return -1;
    while (n < i) { out[n] = directory[n]; ++n; }
    out[n++] = '\\';
    while (start < end) {
        out[n++] = path[start] == '/' ? '\\' : path[start];
        ++start;
    }
    out[n] = 0;
    return 0;
}
#endif
