#pragma once
#include <stdint.h>
typedef int ret_code_t;
typedef struct { uint16_t file_id; uint16_t key; } fds_record_desc_t;
typedef struct { uint16_t length_words; } fds_header_t;
typedef struct {
    const fds_header_t* p_header;
    const void* p_data;
    uint16_t length_words;
} fds_flash_record_t;
typedef struct { uint16_t file_id; uint16_t key; fds_flash_record_t data; } fds_record_t;
typedef struct { uint16_t file_id; uint16_t key; } fds_find_token_t;
#define FDS_SUCCESS 0
static inline ret_code_t fds_init(void) { return FDS_SUCCESS; }
static inline ret_code_t fds_record_find(uint16_t, uint16_t, fds_record_desc_t*, fds_find_token_t*) { return -1; }
static inline ret_code_t fds_record_open(fds_record_desc_t*, fds_flash_record_t* rec) {
    static fds_header_t hdr = {0};
    if (rec) {
        rec->p_header = &hdr;
        rec->p_data = nullptr;
        rec->length_words = 0;
    }
    return -1;
}
static inline ret_code_t fds_record_close(fds_record_desc_t*) { return FDS_SUCCESS; }
static inline ret_code_t fds_record_write(fds_record_desc_t*, fds_record_t const*) { return FDS_SUCCESS; }
static inline ret_code_t fds_record_update(fds_record_desc_t*, fds_record_t const*) { return FDS_SUCCESS; }
