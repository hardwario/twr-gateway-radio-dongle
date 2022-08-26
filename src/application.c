#include <application.h>
#include <radio.h>
#include <usb_talk.h>
#include <eeprom.h>

#define APPLICATION_TASK_ID 0

static uint64_t my_id;
static twr_led_t led;
static bool led_state;
static bool radio_pairing_mode;

static void radio_event_handler(twr_radio_event_t event, void *event_param);
static void led_state_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void led_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void relay_state_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void relay_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void module_relay_state_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void module_relay_pulse(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void module_relay_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void lcd_text_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void lcd_screen_clear(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void led_strip_color_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void led_strip_brightness_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void led_strip_compound_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void led_strip_effect_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void led_strip_thermometer_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);

static void info_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void nodes_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void nodes_purge(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void nodes_add(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void nodes_remove(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void scan_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void scan_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void pairing_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void pairing_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void automatic_pairing_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void automatic_pairing_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);

static void alias_add(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void alias_remove(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);
static void alias_list(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);

static void radio_sub_callback(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub);

const usb_talk_subscribe_t subscribes[] = {
    {"led/-/state/set", led_state_set, 0, NULL},
    {"led/-/state/get", led_state_get, 0, NULL},
    {"relay/-/state/set", relay_state_set, 0, NULL},
    {"relay/-/state/get", relay_state_get, 0, NULL},
    {"relay/0:0/state/set", module_relay_state_set, 0, NULL},
    {"relay/0:0/state/get", module_relay_state_get, 0, NULL},
    {"relay/0:0/pulse/set", module_relay_pulse, 0, NULL},
    {"relay/0:1/state/set", module_relay_state_set, 1, NULL},
    {"relay/0:1/state/get", module_relay_state_get, 1, NULL},
    {"relay/0:1/pulse/set", module_relay_pulse, 1, NULL},
    {"lcd/-/text/set", lcd_text_set, 0, NULL},
    {"lcd/-/screen/clear", lcd_screen_clear, 0, NULL},
    {"led-strip/-/color/set", led_strip_color_set, 0, NULL},
    {"led-strip/-/brightness/set", led_strip_brightness_set, 0, NULL},
    {"led-strip/-/compound/set", led_strip_compound_set, 0, NULL},
    {"led-strip/-/effect/set", led_strip_effect_set, 0, NULL},
    {"led-strip/-/thermometer/set", led_strip_thermometer_set, 0, NULL},
    {"/info/get", info_get, 0, NULL},
    {"/nodes/get", nodes_get, 0, NULL},
    {"/nodes/add", nodes_add, 0, NULL},
    {"/nodes/remove", nodes_remove, 0, NULL},
    {"/nodes/purge", nodes_purge, 0, NULL},
    {"/scan/start", scan_start, 0, NULL},
    {"/scan/stop", scan_stop, 0, NULL},
    {"/pairing-mode/start", pairing_start, 0, NULL},
    {"/pairing-mode/stop", pairing_stop, 0, NULL},
    {"/automatic-pairing/start", automatic_pairing_start, 0, NULL},
    {"/automatic-pairing/stop", automatic_pairing_stop, 0, NULL},
    {"$eeprom/alias/add", alias_add, 0, NULL},
    {"$eeprom/alias/remove", alias_remove, 0, NULL},
    {"$eeprom/alias/list", alias_list, 0, NULL},
};

void application_init(void)
{
    twr_led_init(&led, GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    usb_talk_init();
    usb_talk_subscribes(subscribes, sizeof(subscribes) / sizeof(usb_talk_subscribe_t));

    twr_radio_init(TWR_RADIO_MODE_GATEWAY);
    twr_radio_set_event_handler(radio_event_handler, NULL);

    eeprom_init();

    twr_led_pulse(&led, 2000);

    led_state = false;
}

static void radio_event_handler(twr_radio_event_t event, void *event_param)
{
    (void) event_param;

    uint64_t id = twr_radio_get_event_id();

    if (event == TWR_RADIO_EVENT_ATTACH)
    {
        twr_led_pulse(&led, 1000);

        usb_talk_send_format("[\"/attach\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
    else if (event == TWR_RADIO_EVENT_ATTACH_FAILURE)
    {
        twr_led_pulse(&led, 5000);

        usb_talk_send_format("[\"/attach-failure\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
    else if (event == TWR_RADIO_EVENT_DETACH)
    {
        twr_led_pulse(&led, 1000);

        usb_talk_send_format("[\"/detach\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
    else if (event == TWR_RADIO_EVENT_INIT_DONE)
    {
        my_id = twr_radio_get_my_id();

        info_get(NULL, NULL, NULL);
    }
    else if (event == TWR_RADIO_EVENT_SCAN_FIND_DEVICE)
    {
        usb_talk_send_format("[\"/found\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
}

void twr_radio_pub_on_event_count(uint64_t *id, uint8_t event_id, uint16_t *event_count)
{
    twr_led_pulse(&led, 10);

    if (event_id == TWR_RADIO_PUB_EVENT_PUSH_BUTTON)
    {
        usb_talk_publish_event_count(id, "push-button/-", event_count);
    }
    else if (event_id == TWR_RADIO_PUB_EVENT_PIR_MOTION)
    {
        usb_talk_publish_event_count(id, "pir/-", event_count);
    }
    else if (event_id == TWR_RADIO_PUB_EVENT_LCD_BUTTON_LEFT)
    {
        usb_talk_publish_event_count(id, "push-button/lcd:left", event_count);
    }
    else if (event_id == TWR_RADIO_PUB_EVENT_LCD_BUTTON_RIGHT)
    {
        usb_talk_publish_event_count(id, "push-button/lcd:right", event_count);
    }
    else if (event_id == TWR_RADIO_PUB_EVENT_ACCELEROMETER_ALERT)
    {
        usb_talk_publish_event_count(id, "accelerometer/-", event_count);
    }
    else if (event_id == TWR_RADIO_PUB_EVENT_HOLD_BUTTON)
    {
        usb_talk_send_format("[\"%012llx/push-button/-/hold-count\", %" PRIu16 "]\n", *id, *event_count);
    }
}

void twr_radio_pub_on_temperature(uint64_t *id, uint8_t channel, float *celsius)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_temperature(id, channel, celsius);
}

void twr_radio_pub_on_humidity(uint64_t *id, uint8_t channel, float *percentage)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_humidity(id, channel, percentage);
}

void twr_radio_pub_on_lux_meter(uint64_t *id, uint8_t channel, float *illuminance)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_lux_meter(id, channel, illuminance);
}

void twr_radio_pub_on_barometer(uint64_t *id, uint8_t channel, float *pressure, float *altitude)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_barometer(id, channel, pressure, altitude);
}

void twr_radio_pub_on_co2(uint64_t *id, float *concentration)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_co2(id, concentration);
}

void twr_radio_pub_on_battery(uint64_t *id, float *voltage)
{
    twr_led_pulse(&led, 10);

    usb_talk_message_start_id(id, "battery/-/voltage");

    usb_talk_message_append_float("%.2f", voltage);

    usb_talk_message_send();
}

void twr_radio_pub_on_state(uint64_t *id, uint8_t who, bool *state)
{
    twr_led_pulse(&led, 10);

    static const char *lut[] = {
            [TWR_RADIO_PUB_STATE_LED] = "led/-/state",
            [TWR_RADIO_PUB_STATE_RELAY_MODULE_0] = "relay/0:0/state",
            [TWR_RADIO_PUB_STATE_RELAY_MODULE_1] = "relay/0:1/state",
            [TWR_RADIO_PUB_STATE_POWER_MODULE_RELAY] = "relay/-/state"
    };

    if (who < 4)
    {
        usb_talk_publish_bool(id, lut[who], state);
    }
}

void twr_radio_pub_on_value_int(uint64_t *id, uint8_t value_id, int *value)
{
    twr_led_pulse(&led, 10);

    if (value_id == TWR_RADIO_PUB_VALUE_HOLD_DURATION_BUTTON)
    {
        usb_talk_send_format("[\"%012llx/push-button/-/hold-duration\", %d]\n", *id, *value);
    }
}

void twr_radio_pub_on_acceleration(uint64_t *id, float *x_axis, float *y_axis, float *z_axis)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_accelerometer_acceleration(id, x_axis, y_axis, z_axis);
}

void twr_radio_pub_on_buffer(uint64_t *id, void *buffer, size_t length)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_buffer(id, buffer, length);
}

void twr_radio_on_info(uint64_t *id, char *firmware, char *version, twr_radio_mode_t mode)
{
    twr_led_pulse(&led, 10);

    usb_talk_send_format("[\"" USB_TALK_DEVICE_ADDRESS "/info\", {\"firmware\": \"%s\", \"version\": \"%s\", \"mode\": %d}]\n", *id, firmware, version, mode);
}

void twr_radio_on_sub(uint64_t *id, uint8_t *number, twr_radio_sub_pt_t *pt, char *topic)
{
    twr_led_pulse(&led, 10);

    twr_radio_sub_pt_t payload_type = *pt;

    usb_talk_add_sub(topic, radio_sub_callback, *number, (void *) payload_type); // Small trick, save number as pointer

    usb_talk_send_format("[\"$sub\", {\"topic\": \"" USB_TALK_DEVICE_ADDRESS "/%s\", \"pt\": %d}]\n", *id, topic, *pt);

}

void twr_radio_pub_on_bool(uint64_t *id, char *subtopic, bool *value)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_bool(id, subtopic, value);
}

void twr_radio_pub_on_int(uint64_t *id, char *subtopic, int *value)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_int(id, subtopic, value);
}

void twr_radio_pub_on_float(uint64_t *id, char *subtopic, float *value)
{
    twr_led_pulse(&led, 10);

    usb_talk_publish_float(id, subtopic, value);
}

void twr_radio_pub_on_uint32(uint64_t *id, char *subtopic, uint32_t *value)
{
    twr_led_pulse(&led, 10);

    usb_talk_message_start_id(id, subtopic);

    if (value == NULL)
    {
        usb_talk_message_append("null");
    }
    else
    {
        usb_talk_message_append("%u", *value);
    }

    usb_talk_message_send();
}

void twr_radio_pub_on_string(uint64_t *id, char *subtopic, char *value)
{
    twr_led_pulse(&led, 10);

    usb_talk_message_start_id(id, subtopic);

    usb_talk_message_append("\"%s\"", value);

    usb_talk_message_send();
}

static void led_state_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;
    bool state;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    if (my_id == *id)
    {
        led_state = state;

        twr_led_set_mode(&led, led_state ? TWR_LED_MODE_ON : TWR_LED_MODE_OFF);

        usb_talk_publish_led(&my_id, &led_state);
    }
    else
    {
        twr_radio_node_state_set(id, TWR_RADIO_NODE_STATE_LED, &state);
    }
}

static void led_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) payload;
    (void) sub;

    if (my_id == *id)
    {
        usb_talk_publish_led(&my_id, &led_state);
    }
    else
    {
        twr_radio_node_state_get(id, TWR_RADIO_NODE_STATE_LED);
    }
}

static void relay_state_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;
    bool state;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    if (my_id == *id)
    {
        twr_module_power_relay_set_state(state);

        usb_talk_publish_relay(&my_id, &state);
    }
    else
    {
        twr_radio_node_state_set(id, TWR_RADIO_NODE_STATE_POWER_MODULE_RELAY, &state);
    }
}

static void relay_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) payload;
    (void) sub;

    if (my_id == *id)
    {
        bool state = twr_module_power_relay_get_state();

        usb_talk_publish_relay(&my_id, &state);
    }
    else
    {
        twr_radio_node_state_get(id, TWR_RADIO_NODE_STATE_POWER_MODULE_RELAY);
    }

}

static void module_relay_state_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    bool state;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    if (my_id != *id)
    {
        twr_radio_node_state_set(id, sub->number == 0 ? TWR_RADIO_NODE_STATE_RELAY_MODULE_0 : TWR_RADIO_NODE_STATE_RELAY_MODULE_1, &state);
    }
}

static void module_relay_pulse(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{

    int duration;
    bool direction;

    if (!usb_talk_payload_get_key_int(payload, "duration", &duration))
    {
        duration = 500;
    }

    if (!usb_talk_payload_get_key_bool(payload, "direction", &direction))
    {
        direction = true;
    }

    if (my_id != *id)
    {
        uint8_t buffer[1 + sizeof(uint64_t) + 1 + 4]; // HEAD + ADDRESS + DIRECTION + DURATION(4)
        buffer[0] = (sub->number == 0) ? RADIO_RELAY_0_PULSE_SET : RADIO_RELAY_1_PULSE_SET;
        memcpy(buffer + 1, id, sizeof(uint64_t));
        buffer[sizeof(uint64_t) + 1] = (uint8_t) direction;
        memcpy(&buffer[sizeof(uint64_t) + 2], &duration, sizeof(uint32_t));

        twr_radio_pub_buffer(buffer, sizeof(buffer));
    }
}

static void module_relay_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) payload;

    if (my_id != *id)
    {
        twr_radio_node_state_get(id, sub->number == 0 ? TWR_RADIO_NODE_STATE_RELAY_MODULE_0 : TWR_RADIO_NODE_STATE_RELAY_MODULE_1);
    }
}

static void lcd_text_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;

    int x;
    int y;
    int font_size;
    bool color;
    char text[33];
    size_t length = sizeof(text);

    memset(text, 0, length);
    if (!usb_talk_payload_get_key_int(payload, "x", &x))
    {
        return;
    }
    if (!usb_talk_payload_get_key_int(payload, "y", &y))
    {
        return;
    }
    if (!usb_talk_payload_get_key_string(payload, "text", text, &length))
    {
        return;
    }

    if (!usb_talk_payload_get_key_int(payload, "font", &font_size))
    {
        font_size = 15;
    }

    if (!usb_talk_payload_get_key_bool(payload, "color", &color))
    {
        color = true;
    }

    if (my_id != *id)
    {
        uint8_t buffer[1 + sizeof(uint64_t) + 5 + 32]; // HEAD + ADDRESS + X + Y + FONT_SIZE + LENGTH + TEXT
        buffer[0] = RADIO_LCD_TEXT_SET;
        memcpy(buffer + 1, id, sizeof(uint64_t));
        buffer[sizeof(uint64_t) + 1] = (uint8_t) x;
        buffer[sizeof(uint64_t) + 2] = (uint8_t) y;
        buffer[sizeof(uint64_t) + 3] = (uint8_t) font_size;
        buffer[sizeof(uint64_t) + 4] = (uint8_t) color;
        buffer[sizeof(uint64_t) + 5] = (uint8_t) length;
        memcpy(buffer + sizeof(uint64_t) + 6, text, length + 1);

        twr_radio_pub_buffer(buffer, 1 + sizeof(uint64_t) + 4 + length + 1);
    }
}

static void lcd_screen_clear(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) payload;
    (void) sub;

    if (my_id != *id)
    {
        uint8_t buffer[1 + sizeof(uint64_t)];
        buffer[0] = RADIO_LCD_SCREEN_CLEAR;
        memcpy(buffer + 1, id, sizeof(uint64_t));
        twr_radio_pub_buffer(buffer, sizeof(buffer));
    }
}

static void led_strip_color_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;

    uint32_t color;

    if (!usb_talk_payload_get_color(payload, &color))
    {
        return;
    }

    twr_radio_node_led_strip_color_set(id, color);
}

static void led_strip_brightness_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;

    int value;

    if (!usb_talk_payload_get_int(payload, &value))
    {
        return;
    }

    if ((value < 0) || value > 100)
    {
        return;
    }

    uint8_t brightness = (uint16_t)value * 255 / 100;

    twr_radio_node_led_strip_brightness_set(id, brightness);
}

static void led_strip_compound_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;

    uint8_t compound[TWR_RADIO_NODE_MAX_COMPOUND_BUFFER_SIZE];

    size_t length = sizeof(compound);

    int count_sum;

    usb_talk_payload_get_compound(payload, compound, &length, &count_sum);

    twr_radio_node_led_strip_compound_set(id, compound, length);
}

static void led_strip_effect_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;

    int type;
    int wait;
    uint32_t color;

    if (!usb_talk_payload_get_key_enum(payload, "type", &type, "test", "rainbow", "rainbow-cycle", "theater-chase-rainbow", "color-wipe", "theater-chase", "stroboscope", "icicle", "pulse-color"))
    {
        return;
    }

    if (type != TWR_RADIO_NODE_LED_STRIP_EFFECT_TEST)
    {
        if (!usb_talk_payload_get_key_int(payload, "wait", &wait))
        {
            return;
        }

        if (wait < 0)
        {
            return;
        }
    }

    if ((type == TWR_RADIO_NODE_LED_STRIP_EFFECT_COLOR_WIPE) || (type == TWR_RADIO_NODE_LED_STRIP_EFFECT_THEATER_CHASE) || (type == TWR_RADIO_NODE_LED_STRIP_EFFECT_STROBOSCOPE) || (type == TWR_RADIO_NODE_LED_STRIP_EFFECT_ICICLE) || (type == TWR_RADIO_NODE_LED_STRIP_EFFECT_PULSE_COLOR))
    {
        if (!usb_talk_payload_get_key_color(payload, "color", &color))
        {
            return;
        }
    }

    twr_radio_node_led_strip_effect_set(id, (twr_radio_node_led_strip_effect_t) type, (uint16_t) wait, color);
}

static void led_strip_thermometer_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;

    float temperature;
    int min;
    int max;
    int white_dots = 0;
    float set_point;

    if (!usb_talk_payload_get_key_float(payload, "temperature", &temperature))
    {
        return;
    }

    if (!usb_talk_payload_get_key_int(payload, "min", &min) || (min > 127) || (min < -128))
    {
        return;
    }

    if (!usb_talk_payload_get_key_int(payload, "max", &max) || (max > 127) || (max < -128))
    {
        return;
    }

    usb_talk_payload_get_key_int(payload, "white-dots", &white_dots);

    if((white_dots > 255) || (white_dots < 0))
    {
    	white_dots = 0;
    }

    if (usb_talk_payload_get_key_float(payload, "set-point", &set_point))
    {
        uint32_t color = 0;

        if (!usb_talk_payload_get_key_color(payload, "color", &color))
        {
            return;
        }

        twr_radio_node_led_strip_thermometer_set(id, temperature, min, max, white_dots, &set_point, color);
    }
    else
    {
        twr_radio_node_led_strip_thermometer_set(id, temperature, min, max, white_dots, NULL, 0);
    }
}

static void info_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;
    (void) payload;

    usb_talk_send_format("[\"/info\", {\"id\": \"" USB_TALK_DEVICE_ADDRESS "\", \"firmware\": \"" FIRMWARE "\", \"version\": \"" FW_VERSION "\"}]\n", my_id);
}

static void nodes_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;
    (void) payload;

    uint64_t peer_devices_address[TWR_RADIO_MAX_DEVICES];

    twr_radio_get_peer_id(peer_devices_address, TWR_RADIO_MAX_DEVICES);

    usb_talk_publish_nodes(peer_devices_address, TWR_RADIO_MAX_DEVICES);
}


void _radio_node(usb_talk_payload_t *payload, bool (*call)(uint64_t))
{
    char tmp[13];
    size_t length = sizeof(tmp);

    if (!usb_talk_payload_get_string(payload, tmp, &length))
    {
        return;
    }

    if (length == 12)
    {
        uint64_t id = 0;

        if (sscanf(tmp, "%012llx/", &id))
        {
            call(id);
        }
    }
}

static void nodes_add(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;
    _radio_node(payload, twr_radio_peer_device_add);
}

static void nodes_remove(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;
    _radio_node(payload, twr_radio_peer_device_remove);
}

static void nodes_purge(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;

    twr_radio_peer_device_purge_all();

    nodes_get(id, payload, sub);
}

static void scan_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    twr_radio_scan_start();

    usb_talk_send_string("[\"/scan\", \"start\"]\n");
}

static void scan_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    twr_radio_scan_stop();

    usb_talk_send_string("[\"/scan\", \"stop\"]\n");
}


static void pairing_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    radio_pairing_mode = true;

    twr_led_set_mode(&led, TWR_LED_MODE_BLINK_FAST);

    twr_radio_pairing_mode_start();

    usb_talk_send_string("[\"/pairing-mode\", \"start\"]\n");
}

static void pairing_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    radio_pairing_mode = false;

    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_radio_pairing_mode_stop();

    usb_talk_send_string("[\"/pairing-mode\", \"stop\"]\n");
}

static void automatic_pairing_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    twr_led_set_mode(&led, TWR_LED_MODE_BLINK_FAST);

    twr_radio_automatic_pairing_start();

    usb_talk_send_string("[\"/automatic-pairing\", \"start\"]\n");
}

static void automatic_pairing_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    twr_radio_automatic_pairing_stop();

    usb_talk_send_string("[\"/automatic-pairing\", \"stop\"]\n");
}

static void alias_add(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;

    uint64_t node_id;

    if (!usb_talk_payload_get_key_node_id(payload, "id", &node_id))
    {
        return;
    }

    char name[EEPROM_ALIAS_NAME_LENGTH + 1];
    size_t length = sizeof(name);

    if (!usb_talk_payload_get_key_string(payload, "name", name, &length))
    {
        return;
    }

    eeprom_alias_add(&node_id, name);
}

static void alias_remove(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;

    uint64_t node_id;

    if (!usb_talk_payload_get_node_id(payload, &node_id))
    {
        return;
    }

    eeprom_alias_remove(&node_id);
}

static void alias_list(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;

    int page;

    if (!usb_talk_payload_get_int(payload, &page))
    {
        return;
    }

    eeprom_alias_list(page);
}

static void radio_sub_callback(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    twr_radio_sub_pt_t payload_type = (twr_radio_sub_pt_t) sub->param;

    uint8_t value[41];

    size_t value_size = 0;

    switch (payload_type) {
        case TWR_RADIO_SUB_PT_BOOL:
        {
            if (!usb_talk_payload_get_bool(payload, (bool *) value))
            {
                return;
            }

            value_size = sizeof(bool);

            break;
        }
        case TWR_RADIO_SUB_PT_INT:
        {
            if (!usb_talk_payload_get_int(payload, (int *) value))
            {
                return;
            }

            value_size = sizeof(int);

            break;
        }
        case TWR_RADIO_SUB_PT_FLOAT:
        {
            if (!usb_talk_payload_get_float(payload, (float *) value))
            {
                return;
            }

            value_size = sizeof(float);

            break;
        }
        case TWR_RADIO_SUB_PT_STRING:
        {
            value_size = sizeof(value);

            if (!usb_talk_payload_get_string(payload, (char *) value, &value_size))
            {
                return;
            }

            value_size += 1;

            break;
        }
        case TWR_RADIO_SUB_PT_NULL:
        {
            break;
        }
        default:
        {
            return;
        }
    }

    twr_radio_send_sub_data(id, sub->number, value, value_size);
}
