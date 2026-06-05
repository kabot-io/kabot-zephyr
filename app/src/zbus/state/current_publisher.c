#include "zbus/channels/state_channel.h"
#include "zbus/state_publish_utils.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(current_publisher, LOG_LEVEL_DBG);

BUILD_ASSERT(DT_HAS_ALIAS(kabot_current_left),
             "No devicetree alias 'kabot-current-left' found for current publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_current_left)),
             "Left current devicetree node is not enabled");
BUILD_ASSERT(DT_HAS_ALIAS(kabot_current_right),
             "No devicetree alias 'kabot-current-right' found for current publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_current_right)),
             "Right current devicetree node is not enabled");
BUILD_ASSERT(DT_HAS_ALIAS(kabot_current_supply),
             "No devicetree alias 'kabot-current-supply' found for current publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_current_supply)),
             "Supply current devicetree node is not enabled");

static bool read_ina219_triplet(const struct device *dev,
                                const char *name,
                                float *current,
                                float *bus_voltage,
                                float *power)
{
    struct sensor_value current_value = {0};
    struct sensor_value bus_voltage_value = {0};
    struct sensor_value power_value = {0};

    int rc = sensor_sample_fetch(dev);
    if (rc != 0) {
        LOG_WRN("%s sensor_sample_fetch failed: %d", name, rc);
        return false;
    }

    rc = sensor_channel_get(dev, SENSOR_CHAN_CURRENT, &current_value);
    if (should_skip_invalid_sensor_sample(rc)) {
        LOG_DBG("Skipping invalid %s current sample", name);
        return false;
    }
    if (rc != 0) {
        LOG_WRN("sensor_channel_get(%s, SENSOR_CHAN_CURRENT) failed: %d", name, rc);
        return false;
    }

    rc = sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, &bus_voltage_value);
    if (should_skip_invalid_sensor_sample(rc)) {
        LOG_DBG("Skipping invalid %s voltage sample", name);
        return false;
    }
    if (rc != 0) {
        LOG_WRN("sensor_channel_get(%s, SENSOR_CHAN_VOLTAGE) failed: %d", name, rc);
        return false;
    }

    rc = sensor_channel_get(dev, SENSOR_CHAN_POWER, &power_value);
    if (should_skip_invalid_sensor_sample(rc)) {
        LOG_DBG("Skipping invalid %s power sample", name);
        return false;
    }
    if (rc != 0) {
        LOG_WRN("sensor_channel_get(%s, SENSOR_CHAN_POWER) failed: %d", name, rc);
        return false;
    }

    *current = sensor_value_to_float(&current_value);
    *bus_voltage = sensor_value_to_float(&bus_voltage_value);
    *power = sensor_value_to_float(&power_value);
    return true;
}

void current_publisher_task(void)
{
    const struct device *left = DEVICE_DT_GET(DT_ALIAS(kabot_current_left));
    const struct device *right = DEVICE_DT_GET(DT_ALIAS(kabot_current_right));
    const struct device *supply = DEVICE_DT_GET(DT_ALIAS(kabot_current_supply));

    while (!device_is_ready(left) || !device_is_ready(right) || !device_is_ready(supply)) {
        if (!device_is_ready(left)) {
            LOG_ERR("Left INA219 not ready: %s. Retrying...", left->name);
        }
        if (!device_is_ready(right)) {
            LOG_ERR("Right INA219 not ready: %s. Retrying...", right->name);
        }
        if (!device_is_ready(supply)) {
            LOG_ERR("Supply INA219 not ready: %s. Retrying...", supply->name);
        }
        k_sleep(K_MSEC(CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS));
    }

    LOG_INF("Current publisher active: %d ms (%s, %s, %s)",
            CONFIG_KABOT_STATE_CURRENT_PERIOD_MS,
            left->name,
            right->name,
            supply->name);

    while (true) {
        float left_current = 0.0f;
        float left_bus_voltage = 0.0f;
        float left_power = 0.0f;
        float right_current = 0.0f;
        float right_bus_voltage = 0.0f;
        float right_power = 0.0f;
        float supply_current = 0.0f;
        float supply_bus_voltage = 0.0f;
        float supply_power = 0.0f;

        bool left_valid = read_ina219_triplet(left,
                                              "left",
                                              &left_current,
                                              &left_bus_voltage,
                                              &left_power);
        bool right_valid = read_ina219_triplet(right,
                                               "right",
                                               &right_current,
                                               &right_bus_voltage,
                                               &right_power);
        bool supply_valid = read_ina219_triplet(supply,
                                                "supply",
                                                &supply_current,
                                                &supply_bus_voltage,
                                                &supply_power);

        if (!left_valid && !right_valid && !supply_valid) {
            k_sleep(K_MSEC(CONFIG_KABOT_STATE_CURRENT_PERIOD_MS));
            continue;
        }

        State state = State_init_zero;
        uint64_t stamp = state_now_stamp_ms();

        if (left_valid) {
            state.has_current_left = true;
            state.current_left.has_header = true;
            state.current_left.header.stamp = stamp;
            set_header_frame_id(&state.current_left.header, CONFIG_KABOT_STATE_CURRENT_LEFT_FRAME_ID);
            state.current_left.state = left_current;

            state.has_bus_voltage_left = true;
            state.bus_voltage_left.has_header = true;
            state.bus_voltage_left.header.stamp = stamp;
            set_header_frame_id(&state.bus_voltage_left.header, CONFIG_KABOT_STATE_CURRENT_LEFT_FRAME_ID);
            state.bus_voltage_left.state = left_bus_voltage;

            state.has_power_left = true;
            state.power_left.has_header = true;
            state.power_left.header.stamp = stamp;
            set_header_frame_id(&state.power_left.header, CONFIG_KABOT_STATE_CURRENT_LEFT_FRAME_ID);
            state.power_left.state = left_power;
        }

        if (right_valid) {
            state.has_current_right = true;
            state.current_right.has_header = true;
            state.current_right.header.stamp = stamp;
            set_header_frame_id(&state.current_right.header, CONFIG_KABOT_STATE_CURRENT_RIGHT_FRAME_ID);
            state.current_right.state = right_current;

            state.has_bus_voltage_right = true;
            state.bus_voltage_right.has_header = true;
            state.bus_voltage_right.header.stamp = stamp;
            set_header_frame_id(&state.bus_voltage_right.header, CONFIG_KABOT_STATE_CURRENT_RIGHT_FRAME_ID);
            state.bus_voltage_right.state = right_bus_voltage;

            state.has_power_right = true;
            state.power_right.has_header = true;
            state.power_right.header.stamp = stamp;
            set_header_frame_id(&state.power_right.header, CONFIG_KABOT_STATE_CURRENT_RIGHT_FRAME_ID);
            state.power_right.state = right_power;
        }

        if (supply_valid) {
            state.has_current_supply = true;
            state.current_supply.has_header = true;
            state.current_supply.header.stamp = stamp;
            set_header_frame_id(&state.current_supply.header, CONFIG_KABOT_STATE_CURRENT_SUPPLY_FRAME_ID);
            state.current_supply.state = supply_current;

            state.has_bus_voltage_supply = true;
            state.bus_voltage_supply.has_header = true;
            state.bus_voltage_supply.header.stamp = stamp;
            set_header_frame_id(&state.bus_voltage_supply.header, CONFIG_KABOT_STATE_CURRENT_SUPPLY_FRAME_ID);
            state.bus_voltage_supply.state = supply_bus_voltage;

            state.has_power_supply = true;
            state.power_supply.has_header = true;
            state.power_supply.header.stamp = stamp;
            set_header_frame_id(&state.power_supply.header, CONFIG_KABOT_STATE_CURRENT_SUPPLY_FRAME_ID);
            state.power_supply.state = supply_power;
        }

        int rc = publish_state_msg(&state, K_MSEC(CONFIG_KABOT_STATE_CURRENT_PERIOD_MS));
        if (rc != 0) {
            LOG_WRN("Failed to publish current state: %d", rc);
        }

        k_sleep(K_MSEC(CONFIG_KABOT_STATE_CURRENT_PERIOD_MS));
    }
}

K_THREAD_DEFINE(current_publisher_task_id,
                CONFIG_KABOT_CURRENT_PUBLISHER_STACK_SIZE,
                current_publisher_task,
                NULL,
                NULL,
                NULL,
                4,
                0,
                0);