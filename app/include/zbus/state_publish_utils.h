#pragma once

#include "protos/state_control_msg.pb.h"

#include <pb_encode.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

static inline bool encode_frame_id_cb(pb_ostream_t *stream,
                                      const pb_field_iter_t *field,
                                      void *const *arg)
{
    const char *frame_id = (const char *)(*arg);

    if (frame_id == NULL) {
        frame_id = "";
    }

    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    return pb_encode_string(stream, (const pb_byte_t *)frame_id, strlen(frame_id));
}

static inline void set_header_frame_id(Header *header, const char *frame_id)
{
    /* frame_id must stay valid until the message is encoded. */
    header->frame_id.funcs.encode = encode_frame_id_cb;
    header->frame_id.arg = (void *)frame_id;
}

static inline uint64_t state_now_stamp_ms(void)
{
    return (uint64_t)k_uptime_get();
}

static inline float random_rangef(float min_value, float max_value)
{
    float unit = (float)sys_rand32_get() / (float)UINT32_MAX;
    return min_value + ((max_value - min_value) * unit);
}

static inline bool should_skip_invalid_sensor_sample(int rc)
{
    return rc == -EINVAL;
}
