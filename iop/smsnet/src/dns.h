#ifndef SMSNET_DNS_H
#define SMSNET_DNS_H
/* Builds one "A" question for apName. Returns the packet length, 0 if it does not fit. */
int smsdns_query  ( unsigned char* apBuf, int aCap, const char* apName, unsigned int anID );
/* Reads the first IPv4 address out of a reply to that question. Returns 1 when found. */
int smsdns_answer ( const unsigned char* apBuf, int aLen, unsigned int anID, unsigned char* apIP );
#endif
