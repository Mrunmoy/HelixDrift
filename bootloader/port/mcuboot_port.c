#include <stddef.h>
#include <stdint.h>

#include "bootutil/image.h"
#include "bootutil/sign_key.h"
#include "flash_map_backend/flash_map_backend.h"

#include "ed25519_pub_key.h"

const struct bootutil_key bootutil_keys[] = {
    {
        .key = ed25519_pub_key,
        .len = &ed25519_pub_key_len,
    },
};

const int bootutil_key_cnt = 1;

int flash_area_id_from_multi_image_slot(int image_index, int slot) {
    (void)image_index;
    return flash_area_id_from_image_slot(slot);
}

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret) {
    uint32_t count = 0;

    if (cnt == NULL || ret == NULL || *cnt <= 0) {
        return -1;
    }

    count = (uint32_t)*cnt;
    if (flash_area_get_sectors(idx, &count, (struct flash_sector *)ret) != 0) {
        return -1;
    }

    *cnt = (int)count;
    return 0;
}
