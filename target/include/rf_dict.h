/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Fuzzing dictionary. Two kinds of entries, one per line:
 *   name="token bytes with \xNN escapes"            (AFL-compatible token)
 *   dcmd NAME 0xVALUE size=N dir=to|from|tofrom|none (resfuzz devctl command)
 * '#' starts a comment. dcmd values must be *resolved on the target*
 * (see docs/DESIGN.md "Dictionary"), never guessed on the host.
 */
#ifndef RF_DICT_H
#define RF_DICT_H
#include <stddef.h>
#include <stdint.h>

#define RF_DICT_MAX_TOKENS 512
#define RF_DICT_MAX_TOKEN  64
#define RF_DICT_MAX_DCMDS  256

enum { RF_DIR_NONE = 0, RF_DIR_TO = 1, RF_DIR_FROM = 2, RF_DIR_TOFROM = 3 };

typedef struct { uint8_t len; uint8_t bytes[RF_DICT_MAX_TOKEN]; } rf_token;
typedef struct { char name[48]; uint32_t value; uint32_t size; uint8_t dir; } rf_dcmd;

typedef struct {
    unsigned ntokens, ndcmds;
    rf_token tokens[RF_DICT_MAX_TOKENS];
    rf_dcmd  dcmds[RF_DICT_MAX_DCMDS];
} rf_dict;

void rf_dict_init(rf_dict *d);
/* Parse text; returns number of entries added, or -1 with *err_line set. */
int  rf_dict_parse(rf_dict *d, const char *text, size_t len, int *err_line);
int  rf_dict_load(rf_dict *d, const char *path);
/* Add a token (deduplicated). Returns 1 if added, 0 if present/full. */
int  rf_dict_add_token(rf_dict *d, const void *bytes, size_t len);
int  rf_dict_add_dcmd(rf_dict *d, const char *name, uint32_t value, uint32_t size, uint8_t dir);

#endif
