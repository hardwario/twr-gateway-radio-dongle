#include <application.h>
#include <bc_device_id.h>
#include <radio.h>
#include <usb_talk.h>
#include <eeprom.h>
#if CORE_MODULE
#include <sensors.h>
#endif

#define APPLICATION_TASK_ID 0

static uint64_t my_id;
static bc_led_t led;
static bool led_state;
static bool radio_pairing_mode;

#if CORE_MODULE
static struct
{
    bc_tick_t next_update;
    bool mqtt;
} lcd;

static bc_module_relay_t relay_0_0;
static bc_module_relay_t relay_0_1;

static void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
static void lcd_button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
#endif

static void radio_event_handler(bc_radio_event_t event, void *event_param);
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
    {"$eeprom/alias/list", alias_list, 0, NULL}
};

void application_init(void)
{
    bc_led_init(&led, GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);

    eeprom_init();

    usb_talk_init();
    usb_talk_subscribes(subscribes, sizeof(subscribes) / sizeof(usb_talk_subscribe_t));

    bc_radio_init(BC_RADIO_MODE_GATEWAY);
    bc_radio_set_event_handler(radio_event_handler, NULL);

#if CORE_MODULE
    bc_module_power_init();

    memset(&lcd, 0, sizeof(lcd));

    bc_module_lcd_init(&_bc_module_lcd_framebuffer);
    bc_module_lcd_clear();
    bc_module_lcd_update();

    static bc_button_t button;
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    static bc_button_t lcd_left;
    bc_button_init_virtual(&lcd_left, BC_MODULE_LCD_BUTTON_LEFT, bc_module_lcd_get_button_driver(), false);
    bc_button_set_event_handler(&lcd_left, lcd_button_event_handler, NULL);

    static bc_button_t lcd_right;
    bc_button_init_virtual(&lcd_right, BC_MODULE_LCD_BUTTON_RIGHT, bc_module_lcd_get_button_driver(), false);
    bc_button_set_event_handler(&lcd_right, lcd_button_event_handler, NULL);

    sensors_init_all(&my_id);

    bc_module_relay_init(&relay_0_0, BC_MODULE_RELAY_I2C_ADDRESS_DEFAULT);
    bc_module_relay_init(&relay_0_1, BC_MODULE_RELAY_I2C_ADDRESS_ALTERNATE);
#endif

    bc_led_set_mode(&led, BC_LED_MODE_OFF);
    led_state = false;
}

static void radio_event_handler(bc_radio_event_t event, void *event_param)
{
    (void) event_param;

    uint64_t id = bc_radio_get_event_id();

    if (event == BC_RADIO_EVENT_ATTACH)
    {
        bc_led_pulse(&led, 1000);

        usb_talk_send_format("[\"/attach\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
    else if (event == BC_RADIO_EVENT_ATTACH_FAILURE)
    {
        bc_led_pulse(&led, 5000);

        usb_talk_send_format("[\"/attach-failure\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
    else if (event == BC_RADIO_EVENT_DETACH)
    {
        bc_led_pulse(&led, 1000);

        usb_talk_send_format("[\"/detach\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
    else if (event == BC_RADIO_EVENT_INIT_DONE)
    {
        my_id = bc_radio_get_my_id();
    }
    else if (event == BC_RADIO_EVENT_SCAN_FIND_DEVICE)
    {
        usb_talk_send_format("[\"/found\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", id);
    }
}

void bc_radio_on_event_count(uint64_t *id, uint8_t event_id, uint16_t *event_count)
{
    bc_led_pulse(&led, 10);

    if (event_id == BC_RADIO_EVENT_PUSH_BUTTON)
    {
        usb_talk_publish_event_count(id, "push-button/-", event_count);
    }
    else if (event_id == BC_RADIO_EVENT_PIR_MOTION)
    {
        usb_talk_publish_event_count(id, "pir/-", event_count);
    }
    else if (event_id == BC_RADIO_EVENT_LCD_BUTTON_LEFT)
    {
        usb_talk_publish_event_count(id, "push-button/lcd:left", event_count);
    }
    else if (event_id == BC_RADIO_EVENT_LCD_BUTTON_RIGHT)
    {
        usb_talk_publish_event_count(id, "push-button/lcd:right", event_count);
    }
    else if (event_id == BC_RADIO_EVENT_ACCELEROMETER_ALERT)
    {
        usb_talk_publish_event_count(id, "accelerometer/-", event_count);
    }
}

void bc_radio_on_thermometer(uint64_t *peer_device_address, uint8_t *i2c, float *temperature)
{
    (void) peer_device_address;

    bc_led_pulse(&led, 10);

    usb_talk_publish_thermometer(peer_device_address, i2c, temperature);
}

void bc_radio_on_humidity(uint64_t *peer_device_address, uint8_t *i2c, float *percentage)
{
    (void) peer_device_address;

    bc_led_pulse(&led, 10);

    usb_talk_publish_humidity_sensor(peer_device_address, i2c, percentage);
}

void bc_radio_on_lux_meter(uint64_t *peer_device_address, uint8_t *i2c, float *illuminance)
{
    (void) peer_device_address;

    bc_led_pulse(&led, 10);

    usb_talk_publish_lux_meter(peer_device_address, i2c, illuminance);
}

void bc_radio_on_barometer(uint64_t *peer_device_address, uint8_t *i2c, float *pressure, float *altitude)
{
    (void) peer_device_address;

    bc_led_pulse(&led, 10);

    usb_talk_publish_barometer(peer_device_address, i2c, pressure, altitude);
}

void bc_radio_on_co2(uint64_t *peer_device_address, float *concentration)
{
    (void) peer_device_address;

    bc_led_pulse(&led, 10);

    usb_talk_publish_co2_concentation(peer_device_address, concentration);
}

void bc_radio_on_battery(uint64_t *peer_device_address, uint8_t *format, float *voltage)
{
    bc_led_pulse(&led, 10);

    usb_talk_send_format("[\"%012llx/battery/%s/voltage\", %.2f]\n",
            *peer_device_address,
            *format == 0 ? "standard" : "mini",
            *voltage);
}

void bc_radio_on_state(uint64_t *peer_device_address, uint8_t who, bool *state)
{
    bc_led_pulse(&led, 10);

    static const char *lut[] = {
            [BC_RADIO_STATE_LED] = "led/-/state",
            [BC_RADIO_STATE_RELAY_MODULE_0] = "relay/0:0/state",
            [BC_RADIO_STATE_RELAY_MODULE_1] = "relay/0:1/state",
            [BC_RADIO_STATE_POWER_MODULE_RELAY] = "relay/-/state"
    };

    if (who < 4)
    {
        usb_talk_publish_bool(peer_device_address, lut[who], state);
    }
}

void bc_radio_on_buffer(uint64_t *peer_device_address, uint8_t *buffer, size_t *length)
{
    if (*length < 1)
    {
        return;
    }

    bc_led_pulse(&led, 10);

    if (*length == 5)
    {
        switch (buffer[0])
        {
            case RADIO_THERMOSTAT_SET_POINT_TEMPERATURE:
            {
                float temperature;
                memcpy(&temperature, buffer + 1, sizeof(temperature));
                usb_talk_publish_float(peer_device_address, "thermostat/set-point/temperature", &temperature);
                break;
            }
            default:
            {
                break;
            }

        }
    }
    else if (*length == 13)
    {
        switch (buffer[0])
        {
            case RADIO_ACCELEROMETER_ACCELERATION:
            {
                float x_axis, y_axis, z_axis;
                memcpy(&x_axis, buffer + 1, sizeof(x_axis));
                memcpy(&y_axis, buffer + 1 + sizeof(x_axis), sizeof(y_axis));
                memcpy(&z_axis, buffer + 1 + sizeof(x_axis) + sizeof(y_axis), sizeof(z_axis));
                usb_talk_publish_accelerometer_acceleration(peer_device_address, &x_axis, &y_axis, &z_axis);
                break;
            }
            default:
            {
                break;
            }

        }
    }
}

void bc_radio_on_info(uint64_t *id, char *firmware, char *version)
{
    bc_led_pulse(&led, 10);

    usb_talk_send_format("[\"" USB_TALK_DEVICE_ADDRESS "/info\", {\"firmware\": \"%s\", \"version\": \"%s\"} ]\n", *id, firmware, version);
}

void bc_radio_on_bool(uint64_t *id, char *subtopic, bool *value)
{
    bc_led_pulse(&led, 10);

    usb_talk_publish_bool(id, subtopic, value);
}

void bc_radio_on_int(uint64_t *id, char *subtopic, int *value)
{
    bc_led_pulse(&led, 10);

    usb_talk_publish_int(id, subtopic, value);
}

void bc_radio_on_float(uint64_t *id, char *subtopic, float *value)
{
    bc_led_pulse(&led, 10);

    usb_talk_publish_float(id, subtopic, value);
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

        bc_led_set_mode(&led, led_state ? BC_LED_MODE_ON : BC_LED_MODE_OFF);

        usb_talk_publish_led(&my_id, &led_state);
    }
    else
    {
        bc_radio_node_state_set(id, BC_RADIO_STATE_LED, &state);
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
        bc_radio_node_state_get(id, BC_RADIO_STATE_LED);
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
        bc_module_power_relay_set_state(state);

        usb_talk_publish_relay(&my_id, &state);
    }
    else
    {
        bc_radio_node_state_set(id, BC_RADIO_STATE_POWER_MODULE_RELAY, &state);
    }
}

static void relay_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) payload;
    (void) sub;

    if (my_id == *id)
    {
        bool state = bc_module_power_relay_get_state();

        usb_talk_publish_relay(&my_id, &state);
    }
    else
    {
        bc_radio_node_state_get(id, BC_RADIO_STATE_POWER_MODULE_RELAY);
    }

}

static void module_relay_state_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) payload;

    bool state;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    if (my_id != *id)
    {
        bc_radio_node_state_set(id, sub->number == 0 ? BC_RADIO_STATE_RELAY_MODULE_0 : BC_RADIO_STATE_RELAY_MODULE_1, &state);
    }
#if CORE_MODULE
    else
    {
        bc_module_relay_set_state(sub->number == 0 ? &relay_0_0 : &relay_0_1, state);
        bc_module_relay_state_t relay_state = state ? BC_MODULE_RELAY_STATE_TRUE : BC_MODULE_RELAY_STATE_FALSE;
        usb_talk_publish_module_relay(&my_id, &sub->number, &relay_state);
    }
#endif
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

        bc_radio_pub_buffer(buffer, sizeof(buffer));
    }
#if CORE_MODULE
    else
    {
        bc_module_relay_pulse(sub->number == 0 ? &relay_0_0 : &relay_0_1, direction, duration);
    }
#endif
}

static void module_relay_state_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) payload;

    if (my_id == *id)
    {
        bc_radio_node_state_get(id, sub->number == 0 ? BC_RADIO_STATE_RELAY_MODULE_0 : BC_RADIO_STATE_RELAY_MODULE_1);
    }
#if CORE_MODULE
    else
    {
        bc_module_relay_state_t state = bc_module_relay_get_state(sub->number == 0 ? &relay_0_0 : &relay_0_1);

        usb_talk_publish_module_relay(&my_id, &sub->number, &state);
    }
#endif
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

    if (my_id == *id)
    {
#if CORE_MODULE
        if (!lcd.mqtt)
        {
            bc_module_lcd_clear();
            lcd.mqtt = true;
        }

        switch (font_size)
        {
            case 11:
            {
                bc_module_lcd_set_font(&bc_font_ubuntu_11);
                break;
            }
            case 13:
            {
                bc_module_lcd_set_font(&bc_font_ubuntu_13);
                break;
            }
            case 15:
            {
                bc_module_lcd_set_font(&bc_font_ubuntu_15);
                break;
            }
            case 24:
            {
                bc_module_lcd_set_font(&bc_font_ubuntu_24);
                break;
            }
            case 28:
            {
                bc_module_lcd_set_font(&bc_font_ubuntu_28);
                break;
            }
            case 33:
            {
                bc_module_lcd_set_font(&bc_font_ubuntu_33);
                break;
            }
            default:
            {
                bc_module_lcd_set_font(&bc_font_ubuntu_15);
                break;
            }
        }

        bc_module_lcd_draw_string(x, y, text, color);
#endif
    }
    else
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

        bc_radio_pub_buffer(buffer, 1 + sizeof(uint64_t) + 4 + length + 1);
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
        bc_radio_pub_buffer(buffer, sizeof(buffer));
    }
#if CORE_MODULE
    else
    {
        bc_module_lcd_clear();
    }
#endif
}

static void led_strip_color_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;
    uint8_t buffer[1 + sizeof(uint64_t) + 4];
    buffer[0] = RADIO_LED_STRIP_COLOR_SET;
    memcpy(buffer + 1, id, sizeof(uint64_t));

    char str[12];
    size_t length = sizeof(str);
    if (!usb_talk_payload_get_string(payload, str, &length))
    {
        return;
    }

    if (((length != 7) && (length != 11)) || (str[0] != '#'))
    {
        return;
    }

    buffer[sizeof(uint64_t) + 1] = usb_talk_hex_to_u8(str + 1);
    buffer[sizeof(uint64_t) + 2] = usb_talk_hex_to_u8(str + 3);
    buffer[sizeof(uint64_t) + 3] = usb_talk_hex_to_u8(str + 5);
    buffer[sizeof(uint64_t) + 4] = (length == 11) ? usb_talk_hex_to_u8(str + 8) : 0x00;

    bc_radio_pub_buffer(buffer, sizeof(buffer));
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

    uint8_t buffer[1 + sizeof(uint64_t) + 1];
    buffer[0] = RADIO_LED_STRIP_BRIGHTNESS_SET;
    memcpy(buffer + 1, id, sizeof(uint64_t));
    buffer[sizeof(uint64_t) + 1] = value;

    bc_radio_pub_buffer(buffer, sizeof(buffer));
}

static void led_strip_compound_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;
    uint8_t buffer[64 - 9];
    buffer[0] = RADIO_LED_STRIP_COMPOUND_SET;
    memcpy(buffer + 1, id, sizeof(uint64_t));

    int count_sum = 0;
    size_t length;

    do
    {
        length = sizeof(buffer) - sizeof(uint64_t) - 2;

        buffer[sizeof(uint64_t) + 1] = count_sum;

        usb_talk_payload_get_compound_buffer(payload, buffer + sizeof(uint64_t) + 2, &length, &count_sum);

        bc_radio_pub_buffer(buffer, length + sizeof(uint64_t) + 2);

    } while ((length == 45) && (count_sum < 255));

}

static void led_strip_effect_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;
    uint8_t buffer[1 + sizeof(uint64_t) + 1 + sizeof(uint16_t) + sizeof(uint32_t)];
    int type;
    int int_wait;
    uint16_t wait;

    buffer[0] = RADIO_LED_STRIP_EFFECT_SET;
    memcpy(buffer + 1, id, sizeof(uint64_t));

    if (!usb_talk_payload_get_key_enum(payload, "type", &type, "test", "rainbow", "rainbow-cycle", "theater-chase-rainbow", "color-wipe", "theater-chase"))
    {
        return;
    }

    buffer[1 + sizeof(uint64_t)] = (uint8_t) type;

    if (type > RADIO_LED_STRIP_EFFECT_TYPE_TEST)
    {
        if (!usb_talk_payload_get_key_int(payload, "wait", &int_wait))
        {
            return;
        }

        if (int_wait < 0)
        {
            return;
        }

        wait = (uint16_t) int_wait;

        memcpy(buffer + 1 + sizeof(uint64_t) + 1, &wait, sizeof(wait));
    }

    if (type > RADIO_LED_STRIP_EFFECT_TYPE_THEATER_CHASE_RAINBOW)
    {
        char str[13];
        size_t length = sizeof(str);
        if (!usb_talk_payload_get_key_string(payload, "color", str, &length))
        {
            return;
        }

        if (((length != 7) && (length != 11)) || (str[0] != '#'))
        {
            return;
        }

        buffer[1 + sizeof(uint64_t) + 1 + sizeof(uint16_t) + 3] = usb_talk_hex_to_u8(str + 1);
        buffer[1 + sizeof(uint64_t) + 1 + sizeof(uint16_t) + 2] = usb_talk_hex_to_u8(str + 3);
        buffer[1 + sizeof(uint64_t) + 1 + sizeof(uint16_t) + 1] = usb_talk_hex_to_u8(str + 5);
        buffer[1 + sizeof(uint64_t) + 1 + sizeof(uint16_t) + 0] = (length == 11) ? usb_talk_hex_to_u8(str + 8) : 0x00;
    }

    bc_radio_pub_buffer(buffer, sizeof(buffer));
}

static void led_strip_thermometer_set(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) sub;

    float temperature;
    int min;
    int max;
    uint8_t buffer[1 + sizeof(uint64_t) + sizeof(float) + 1 + 1];

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

    buffer[0] = RADIO_LED_STRIP_THERMOMETER_SET;
    memcpy(buffer + 1, id, sizeof(uint64_t));

    memcpy(buffer + 1 + sizeof(uint64_t), &temperature, sizeof(temperature));

    buffer[1 + sizeof(uint64_t) + sizeof(uint32_t)] = (int8_t) min;
    buffer[1 + sizeof(uint64_t) + sizeof(uint32_t) + 1] = (int8_t) max;

    bc_radio_pub_buffer(buffer, sizeof(buffer));
}

static void info_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;
    (void) payload;

    usb_talk_send_format("[\"/info\", {\"id\": \"" USB_TALK_DEVICE_ADDRESS "\", \"firmware\": \"" FIRMWARE "\"}]\n", my_id);
}

static void nodes_get(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;
    (void) payload;

    uint64_t peer_devices_address[BC_RADIO_MAX_DEVICES];

    bc_radio_get_peer_id(peer_devices_address, BC_RADIO_MAX_DEVICES);

    usb_talk_publish_nodes(peer_devices_address, BC_RADIO_MAX_DEVICES);
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
    _radio_node(payload, bc_radio_peer_device_add);
}

static void nodes_remove(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) sub;
    _radio_node(payload, bc_radio_peer_device_remove);
}

static void nodes_purge(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;

    bc_radio_peer_device_purge_all();

    nodes_get(id, payload, sub);
}

static void scan_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    bc_radio_scan_start();

    usb_talk_send_string("[\"/scan\", \"start\"]\n");
}

static void scan_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    bc_radio_scan_stop();

    usb_talk_send_string("[\"/scan\", \"stop\"]\n");
}


static void pairing_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    radio_pairing_mode = true;

    bc_led_set_mode(&led, BC_LED_MODE_BLINK_FAST);

    bc_radio_pairing_mode_start();

    usb_talk_send_string("[\"/pairing-mode\", \"start\"]\n");
}

static void pairing_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    radio_pairing_mode = false;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_radio_pairing_mode_stop();

    usb_talk_send_string("[\"/pairing-mode\", \"stop\"]\n");
}

static void automatic_pairing_start(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    bc_led_set_mode(&led, BC_LED_MODE_BLINK_FAST);

    bc_radio_automatic_pairing_start();

    usb_talk_send_string("[\"/automatic-pairing\", \"stop\"]\n");
}

static void automatic_pairing_stop(uint64_t *id, usb_talk_payload_t *payload, usb_talk_subscribe_t *sub)
{
    (void) id;
    (void) payload;
    (void) sub;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_radio_automatic_pairing_stop();

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

#if CORE_MODULE
void application_task(void)
{
    bc_tick_t now = bc_tick_get();
    if (lcd.next_update < now)
    {
        bc_module_lcd_update();
        lcd.next_update = now + 500;
    }

    bc_scheduler_plan_current_relative(500);
}

static void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        static uint16_t event_count = 0;
        usb_talk_publish_push_button(&my_id, "-", &event_count);
        event_count++;
        bc_led_pulse(&led, 100);
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        if (radio_pairing_mode)
        {
            radio_pairing_mode = false;
            bc_radio_pairing_mode_stop();
            bc_led_set_mode(&led, BC_LED_MODE_OFF);
            usb_talk_send_string("[\"/pairing-mode\", \"stop\"]\n");
        }
        else{
            radio_pairing_mode = true;
            bc_radio_pairing_mode_start();
            bc_led_set_mode(&led, BC_LED_MODE_BLINK_FAST);
            usb_talk_send_string("[\"/pairing-mode\", \"start\"]\n");
        }
    }
}

static void lcd_button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) event_param;

    if (event != BC_BUTTON_EVENT_CLICK)
    {
        return;
    }

    if (self->_channel.virtual_channel == BC_MODULE_LCD_BUTTON_LEFT)
    {
        static uint16_t event_left_count = 0;
        usb_talk_publish_push_button(&my_id, "lcd:left", &event_left_count);
        event_left_count++;
    }
    else
    {
        static uint16_t event_right_count = 0;
        usb_talk_publish_push_button(&my_id, "lcd:right", &event_right_count);
        event_right_count++;
    }
}
#endif
