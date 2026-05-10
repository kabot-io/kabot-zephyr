#include "zbus/encoder_publisher.h"

#include "zbus/sensor_channel.h"
#include "zbus/sensor_msg.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/rtio/rtio.h>

#define QDEC_NODE DT_ALIAS(qdec0)

BUILD_ASSERT(DT_NODE_EXISTS(QDEC_NODE),
             "Board overlay must provide a 'qdec0' alias pointing to a multi-unit qdec device.");

#define QDEC_UNIT_CHAN(node_id)                                                                    \
    {.chan_type = SENSOR_CHAN_ROTATION, .chan_idx = DT_REG_ADDR(node_id)},

SENSOR_DT_READ_IODEV(qdec_iodev, QDEC_NODE, DT_FOREACH_CHILD(QDEC_NODE, QDEC_UNIT_CHAN));
RTIO_DEFINE_WITH_MEMPOOL(qdec_rtio, 4, 4, 4, 256, sizeof(void *));

/* Unit order follows DT child reg values, e.g. unit0@0, unit1@1. */
static const uint8_t unit_indices[] = {DT_FOREACH_CHILD_SEP(QDEC_NODE, DT_REG_ADDR, (, ))};

BUILD_ASSERT(ARRAY_SIZE(unit_indices) == 2, "Encoder publisher requires exactly two qdec units.");

LOG_MODULE_REGISTER(encoder_publisher, LOG_LEVEL_DBG);

void encoder_publisher_task(void)
{
    const struct device *dev = DEVICE_DT_GET(QDEC_NODE);
    const struct sensor_decoder_api *decoder;
    uint8_t buf[256];

    if (!device_is_ready(dev)) {
        LOG_ERR("qdec device not ready");
        return;
    }
    if (sensor_get_decoder(dev, &decoder) != 0) {
        LOG_ERR("qdec decoder unavailable");
        return;
    }

    LOG_INF("Encoder publisher started with %u unit(s)", (unsigned int)ARRAY_SIZE(unit_indices));

    while (1) {
        bool decoded_ok[2] = {false, false};
        struct sensor_q31_data decoded[2] = {0};
        int rc = sensor_read(&qdec_iodev, &qdec_rtio, buf, sizeof(buf));

        if (rc != 0) {
            LOG_WRN("sensor_read failed: %d", rc);
            k_sleep(K_SECONDS(1));
            continue;
        }

        for (size_t i = 0; i < ARRAY_SIZE(unit_indices); i++) {
            struct sensor_chan_spec spec = {
                    .chan_type = SENSOR_CHAN_ROTATION,
                    .chan_idx = unit_indices[i],
            };
            struct sensor_q31_data data = {0};
            uint32_t fit = 0;

            rc = decoder->decode(buf, spec, &fit, 1, &data);
            if (rc < 1) {
                continue;
            }

            LOG_DBG("Encoder unit %u: q31=%d shift=%d", unit_indices[i],
                    (int)data.readings[0].value, data.shift);

            if (i < ARRAY_SIZE(decoded)) {
                decoded[i] = data;
                decoded_ok[i] = true;
            }
        }

        if (decoded_ok[0] && decoded_ok[1]) {
            struct sensor_msg msg = {
                    .left_encoder = decoded[0],
                    .right_encoder = decoded[1],
            };
            int pub_rc = publish_sensor_msg(&msg, K_MSEC(20));
            if (pub_rc != 0) {
                LOG_WRN("Failed to publish encoder tuple: %d", pub_rc);
            }
        } else {
            LOG_WRN("Skipping publish: missing decoded data for one or more encoder channels");
        }

        k_sleep(K_MSEC(100));
    }
}

K_THREAD_DEFINE(encoder_publisher_task_id, CONFIG_MAIN_STACK_SIZE, encoder_publisher_task, NULL,
                NULL, NULL, 4, 0, 0);
