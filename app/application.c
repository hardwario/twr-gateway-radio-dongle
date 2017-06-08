#include <application.h>
#include <usb_talk.h>
#include <bc_device_id.h>

#define UPDATE_INTERVAL 5000
#define APPLICATION_TASK_ID 0

static uint64_t my_device_address;
static bc_led_t led;
static bool led_state;

static struct {
    bc_tick_t next_update;
    bool mqtt;

} lcd;

static bc_module_relay_t relay_0_0;
static bc_module_relay_t relay_0_1;

static void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
static void radio_event_handler(bc_radio_event_t event, void *event_param);
static void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param);
static void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param);
static void lux_meter_event_handler(bc_tag_lux_meter_t *self, bc_tag_lux_meter_event_t event, void *event_param);
static void barometer_tag_event_handler(bc_tag_barometer_t *self, bc_tag_barometer_event_t event, void *event_param);
static void co2_event_handler(bc_module_co2_event_t event, void *event_param);
static void encoder_event_handler(bc_module_encoder_event_t event, void *param);

static void led_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void led_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void module_relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void module_relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void lcd_text_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);

void application_init(void)
{
    usb_talk_init();

    bc_led_init(&led, BC_GPIO_LED, false, false);

    bc_module_power_init();

    memset(&lcd, 0, sizeof(lcd));

    bc_module_lcd_init(&_bc_module_lcd_framebuffer);
    bc_module_lcd_clear();
    bc_module_lcd_update();

    static bc_button_t button;
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize radio
    bc_radio_init();
    bc_radio_set_event_handler(radio_event_handler, NULL);
    bc_radio_listen();

    // Tags
    static bc_tag_temperature_t temperature_tag_0_48;
    bc_tag_temperature_init(&temperature_tag_0_48, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT);
    bc_tag_temperature_set_update_interval(&temperature_tag_0_48, UPDATE_INTERVAL);
    static uint8_t temperature_tag_0_48_i2c = (BC_I2C_I2C0 << 7) | BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT;
    bc_tag_temperature_set_event_handler(&temperature_tag_0_48, temperature_tag_event_handler, &temperature_tag_0_48_i2c);

    static bc_tag_temperature_t temperature_tag_0_49;
    bc_tag_temperature_init(&temperature_tag_0_49, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);
    bc_tag_temperature_set_update_interval(&temperature_tag_0_49, UPDATE_INTERVAL);
    static uint8_t temperature_tag_0_49_i2c = (BC_I2C_I2C0 << 7) | BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE;
    bc_tag_temperature_set_event_handler(&temperature_tag_0_49, temperature_tag_event_handler, &temperature_tag_0_49_i2c);

    static bc_tag_temperature_t temperature_tag_1_48;
    bc_tag_temperature_init(&temperature_tag_1_48, BC_I2C_I2C1, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT);
    bc_tag_temperature_set_update_interval(&temperature_tag_1_48, UPDATE_INTERVAL);
    static uint8_t temperature_tag_1_48_i2c = (BC_I2C_I2C1 << 7) | BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT;
    bc_tag_temperature_set_event_handler(&temperature_tag_1_48, temperature_tag_event_handler,&temperature_tag_1_48_i2c);

    static bc_tag_temperature_t temperature_tag_1_49;
    bc_tag_temperature_init(&temperature_tag_1_49, BC_I2C_I2C1, BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);
    bc_tag_temperature_set_update_interval(&temperature_tag_1_49, UPDATE_INTERVAL);
    static uint8_t temperature_tag_1_49_i2c = (BC_I2C_I2C1 << 7) | BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE;
    bc_tag_temperature_set_event_handler(&temperature_tag_1_49, temperature_tag_event_handler,&temperature_tag_1_49_i2c);

    //----------------------------

    static bc_tag_humidity_t humidity_tag_r2_0_40;
    bc_tag_humidity_init(&humidity_tag_r2_0_40, BC_TAG_HUMIDITY_REVISION_R2, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity_tag_r2_0_40, UPDATE_INTERVAL);
    static uint8_t humidity_tag_r2_0_40_i2c = (BC_I2C_I2C0 << 7) | 0x40;
    bc_tag_humidity_set_event_handler(&humidity_tag_r2_0_40, humidity_tag_event_handler, &humidity_tag_r2_0_40_i2c);

    static bc_tag_humidity_t humidity_tag_r2_0_41;
    bc_tag_humidity_init(&humidity_tag_r2_0_41, BC_TAG_HUMIDITY_REVISION_R2, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_ALTERNATE);
    bc_tag_humidity_set_update_interval(&humidity_tag_r2_0_41, UPDATE_INTERVAL);
    static uint8_t humidity_tag_r2_0_41_i2c = (BC_I2C_I2C0 << 7) | 0x41;
    bc_tag_humidity_set_event_handler(&humidity_tag_r2_0_41, humidity_tag_event_handler, &humidity_tag_r2_0_41_i2c);

    static bc_tag_humidity_t humidity_tag_r1_0_5f;
    bc_tag_humidity_init(&humidity_tag_r1_0_5f, BC_TAG_HUMIDITY_REVISION_R1, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity_tag_r1_0_5f, UPDATE_INTERVAL);
    static uint8_t humidity_tag_r1_0_5f_i2c = (BC_I2C_I2C0 << 7) | 0x5f;
    bc_tag_humidity_set_event_handler(&humidity_tag_r1_0_5f, humidity_tag_event_handler, &humidity_tag_r1_0_5f_i2c);

    static bc_tag_humidity_t humidity_tag_r2_1_40;
    bc_tag_humidity_init(&humidity_tag_r2_1_40, BC_TAG_HUMIDITY_REVISION_R2, BC_I2C_I2C1, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity_tag_r2_1_40, UPDATE_INTERVAL);
    static uint8_t humidity_tag_r2_1_40_i2c = (BC_I2C_I2C1 << 7) | 0x40;
    bc_tag_humidity_set_event_handler(&humidity_tag_r2_1_40, humidity_tag_event_handler, &humidity_tag_r2_1_40_i2c);

    static bc_tag_humidity_t humidity_tag_r2_1_41;
    bc_tag_humidity_init(&humidity_tag_r2_1_41, BC_TAG_HUMIDITY_REVISION_R2, BC_I2C_I2C1, BC_TAG_HUMIDITY_I2C_ADDRESS_ALTERNATE);
    bc_tag_humidity_set_update_interval(&humidity_tag_r2_1_41, UPDATE_INTERVAL);
    static uint8_t humidity_tag_r2_1_41_i2c = (BC_I2C_I2C1 << 7) | 0x41;
    bc_tag_humidity_set_event_handler(&humidity_tag_r2_1_41, humidity_tag_event_handler, &humidity_tag_r2_1_41_i2c);

    static bc_tag_humidity_t humidity_tag_r1_1_5f;
    bc_tag_humidity_init(&humidity_tag_r1_1_5f, BC_TAG_HUMIDITY_REVISION_R1, BC_I2C_I2C1, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity_tag_r1_1_5f, UPDATE_INTERVAL);
    static uint8_t humidity_tag_r1_1_5f_i2c = (BC_I2C_I2C1 << 7) | 0x5f;
    bc_tag_humidity_set_event_handler(&humidity_tag_r1_1_5f, humidity_tag_event_handler, &humidity_tag_r1_1_5f_i2c);

    //----------------------------

    static bc_tag_lux_meter_t lux_meter_0_44;
    bc_tag_lux_meter_init(&lux_meter_0_44, BC_I2C_I2C0, BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT);
    bc_tag_lux_meter_set_update_interval(&lux_meter_0_44, UPDATE_INTERVAL);
    static uint8_t lux_meter_0_44_i2c = (BC_I2C_I2C0 << 7) | BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT;
    bc_tag_lux_meter_set_event_handler(&lux_meter_0_44, lux_meter_event_handler, &lux_meter_0_44_i2c);

    static bc_tag_lux_meter_t lux_meter_0_45;
    bc_tag_lux_meter_init(&lux_meter_0_45, BC_I2C_I2C0, BC_TAG_LUX_METER_I2C_ADDRESS_ALTERNATE);
    bc_tag_lux_meter_set_update_interval(&lux_meter_0_45, UPDATE_INTERVAL);
    static uint8_t lux_meter_0_45_i2c = (BC_I2C_I2C0 << 7) | BC_TAG_LUX_METER_I2C_ADDRESS_ALTERNATE;
    bc_tag_lux_meter_set_event_handler(&lux_meter_0_45, lux_meter_event_handler, &lux_meter_0_45_i2c);

    static bc_tag_lux_meter_t lux_meter_1_44;
    bc_tag_lux_meter_init(&lux_meter_1_44, BC_I2C_I2C1, BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT);
    bc_tag_lux_meter_set_update_interval(&lux_meter_1_44, UPDATE_INTERVAL);
    static uint8_t lux_meter_1_44_i2c = (BC_I2C_I2C1 << 7) | BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT;
    bc_tag_lux_meter_set_event_handler(&lux_meter_1_44, lux_meter_event_handler, &lux_meter_1_44_i2c);

    static bc_tag_lux_meter_t lux_meter_1_45;
    bc_tag_lux_meter_init(&lux_meter_1_45, BC_I2C_I2C1, BC_TAG_LUX_METER_I2C_ADDRESS_ALTERNATE);
    bc_tag_lux_meter_set_update_interval(&lux_meter_1_45, UPDATE_INTERVAL);
    static uint8_t lux_meter_1_45_i2c = (BC_I2C_I2C1 << 7) | BC_TAG_LUX_METER_I2C_ADDRESS_ALTERNATE;
    bc_tag_lux_meter_set_event_handler(&lux_meter_1_45, lux_meter_event_handler, &lux_meter_1_45_i2c);

    //----------------------------

    static bc_tag_barometer_t barometer_tag_0;
    bc_tag_barometer_init(&barometer_tag_0, BC_I2C_I2C0);
    bc_tag_barometer_set_update_interval(&barometer_tag_0, UPDATE_INTERVAL);
    static uint8_t barometer_tag_0_i2c = (BC_I2C_I2C0 << 7) | 0x60;
    bc_tag_barometer_set_event_handler(&barometer_tag_0, barometer_tag_event_handler, &barometer_tag_0_i2c);

    static bc_tag_barometer_t barometer_tag_1;
    bc_tag_barometer_init(&barometer_tag_1, BC_I2C_I2C1);
    bc_tag_barometer_set_update_interval(&barometer_tag_1, UPDATE_INTERVAL);
    static uint8_t barometer_tag_1_i2c = (BC_I2C_I2C1 << 7) | 0x60;
    bc_tag_barometer_set_event_handler(&barometer_tag_1, barometer_tag_event_handler, &barometer_tag_1_i2c);

    //----------------------------

    bc_module_co2_init();
    bc_module_co2_set_update_interval(30000);
    bc_module_co2_set_event_handler(co2_event_handler, NULL);

    // ---------------------------

    bc_module_encoder_init();
    bc_module_encoder_set_event_handler(encoder_event_handler, NULL);

    //----------------------------

    bc_module_relay_init(&relay_0_0, BC_MODULE_RELAY_I2C_ADDRESS_DEFAULT);
    bc_module_relay_init(&relay_0_1, BC_MODULE_RELAY_I2C_ADDRESS_ALTERNATE);

    usb_talk_sub("/led/-/state/set", led_state_set, NULL);
    usb_talk_sub("/led/-/state/get", led_state_get, NULL);
    usb_talk_sub("/relay/-/state/set", relay_state_set, NULL);
    usb_talk_sub("/relay/-/state/get", relay_state_get, NULL);
    usb_talk_sub("/relay/0:0/state/set", module_relay_state_set, &relay_0_0);
    usb_talk_sub("/relay/0:0/state/get", module_relay_state_get, &relay_0_0);
    usb_talk_sub("/relay/0:1/state/set", module_relay_state_set, &relay_0_1);
    usb_talk_sub("/relay/0:1/state/get", module_relay_state_get, &relay_0_1);
    usb_talk_sub("/lcd/-/text/set", lcd_text_set, NULL);


}

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
        usb_talk_publish_push_button(&my_device_address, &event_count);
        event_count++;
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        bc_radio_enrollment_start();
        bc_led_set_mode(&led, BC_LED_MODE_BLINK_FAST);
    }
}

static void radio_event_handler(bc_radio_event_t event, void *event_param)
{
    (void) event_param;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    uint64_t peer_device_address = bc_radio_get_event_device_address();

    if (event == BC_RADIO_EVENT_ATTACH)
    {
        bc_led_pulse(&led, 1000);

        usb_talk_publish_radio(&my_device_address, "attach", &peer_device_address);

        bc_radio_enroll_to_gateway();
    }
    else if (event == BC_RADIO_EVENT_ATTACH_FAILURE)
    {
        bc_led_pulse(&led, 5000);

        usb_talk_publish_radio(&my_device_address, "attach-failure", &peer_device_address);
    }
    else if (event == BC_RADIO_EVENT_DETACH)
    {
        bc_led_pulse(&led, 1000);

        usb_talk_publish_radio(&my_device_address, "detach", &peer_device_address);
    }
    else if (event == BC_RADIO_EVENT_INIT_DONE)
    {
        my_device_address = bc_radio_get_device_address();
    }
}

void bc_radio_on_push_button(uint64_t *peer_device_address, uint16_t *event_count)
{
    usb_talk_publish_push_button(peer_device_address, event_count);
}

void bc_radio_on_thermometer(uint64_t *peer_device_address, uint8_t *i2c, float *temperature)
{
    (void) peer_device_address;

    usb_talk_publish_thermometer(peer_device_address, i2c, temperature);
}


void bc_radio_on_humidity(uint64_t *peer_device_address, uint8_t *i2c, float *percentage)
{
    (void) peer_device_address;

    usb_talk_publish_humidity_sensor(peer_device_address, i2c, percentage);
}

void bc_radio_on_lux_meter(uint64_t *peer_device_address, uint8_t *i2c, float *illuminance)
{
    (void) peer_device_address;

    usb_talk_publish_lux_meter(peer_device_address, i2c, illuminance);
}

void bc_radio_on_barometer(uint64_t *peer_device_address, uint8_t *i2c, float *pressure, float *altitude)
{
    (void) peer_device_address;

    usb_talk_publish_barometer(peer_device_address, i2c, pressure, altitude);
}

void bc_radio_on_co2(uint64_t *peer_device_address, float *concentration)
{
    (void) peer_device_address;

    usb_talk_publish_co2_concentation(peer_device_address, concentration);
}

void bc_radio_on_buffer(uint64_t *peer_device_address, uint8_t *buffer, size_t *length)
{
    (void) peer_device_address;

    if (*length < 1 + sizeof(int))
    {
        return;
    }

    if (buffer[0] == 0x00)
    {
        int increment;
        memcpy(&increment, &buffer[1], sizeof(increment));

        usb_talk_publish_encoder(peer_device_address, &increment);
    }
}

static void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    float value;

    if (event != BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_temperature_get_temperature_celsius(self, &value))
    {
        usb_talk_publish_thermometer(&my_device_address, (uint8_t *)event_param, &value);
    }
}

static void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param)
{
    float value;

    if (event != BC_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_humidity_get_humidity_percentage(self, &value))
    {
        usb_talk_publish_humidity_sensor(&my_device_address, (uint8_t *)event_param, &value);
    }
}

static void lux_meter_event_handler(bc_tag_lux_meter_t *self, bc_tag_lux_meter_event_t event, void *event_param)
{
    float value;

    if (event != BC_TAG_LUX_METER_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_lux_meter_get_illuminance_lux(self, &value))
    {
        usb_talk_publish_lux_meter(&my_device_address, (uint8_t *)event_param, &value);
    }
}

static void barometer_tag_event_handler(bc_tag_barometer_t *self, bc_tag_barometer_event_t event, void *event_param)
{
    float pascal;
    float meter;

    if (event != BC_TAG_BAROMETER_EVENT_UPDATE)
    {
        return;
    }

    if (!bc_tag_barometer_get_pressure_pascal(self, &pascal))
    {
        return;
    }

    if (!bc_tag_barometer_get_altitude_meter(self, &meter))
    {
        return;
    }

    usb_talk_publish_barometer(&my_device_address, (uint8_t *)event_param, &pascal, &meter);
}

void co2_event_handler(bc_module_co2_event_t event, void *event_param)
{
    (void) event_param;
    float value;

    if (event == BC_MODULE_CO2_EVENT_UPDATE)
    {
        if (bc_module_co2_get_concentration(&value))
        {
            usb_talk_publish_co2_concentation(&my_device_address, &value);
        }
    }
}

static void encoder_event_handler(bc_module_encoder_event_t event, void *param)
{
    (void)param;

    if (event == BC_MODULE_ENCODER_EVENT_ROTATION)
    {
        int increment = bc_module_encoder_get_increment();
        usb_talk_publish_encoder(&my_device_address, &increment);
    }
}

static void led_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) param;

    if (my_device_address != *device_address)
    {
    	return;
    }

    if (!usb_talk_payload_get_bool(payload, &led_state))
    {
        return;
    }

    bc_led_set_mode(&led, led_state ? BC_LED_MODE_ON : BC_LED_MODE_OFF);

    usb_talk_publish_led(&my_device_address, &led_state);
}

static void led_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;
    (void) param;

    if (my_device_address != *device_address)
	{
		return;
	}

    usb_talk_publish_led(&my_device_address, &led_state);
}

static void relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) param;
    bool state;

    if (my_device_address != *device_address)
	{
		return;
	}

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    bc_module_power_relay_set_state(state);

    usb_talk_publish_relay(&my_device_address, &state);
}

static void relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;
    (void) param;

    if (my_device_address != *device_address)
	{
		return;
	}

    bool state = bc_module_power_relay_get_state();

    usb_talk_publish_relay(&my_device_address, &state);
}

static void module_relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;

    if (my_device_address != *device_address)
	{
		return;
	}

    bc_module_relay_t *relay = (bc_module_relay_t *)param;

    bool state;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    bc_module_relay_set_state(relay, state);

    uint8_t number = (&relay_0_0 == relay) ? 0 : 1;
    bc_module_relay_state_t relay_state = state ? BC_MODULE_RELAY_STATE_TRUE : BC_MODULE_RELAY_STATE_FALSE;

    usb_talk_publish_module_relay(&my_device_address, &number, &relay_state);
}

static void module_relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;

    if (my_device_address != *device_address)
	{
		return;
	}

    bc_module_relay_t *relay = (bc_module_relay_t *)param;

    bc_module_relay_state_t state = bc_module_relay_get_state(relay);

    uint8_t number = (&relay_0_0 == relay) ? 0 : 1;

    usb_talk_publish_module_relay(&my_device_address, &number, &state);
}

static void lcd_text_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) param;

    if (my_device_address != *device_address)
	{
		return;
	}

    int x;
    int y;
    int font_size = 0;
    char text[32];
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

    if (!lcd.mqtt)
    {
        bc_module_lcd_clear();
        lcd.mqtt = true;
    }

    usb_talk_payload_get_key_int(payload, "font", &font_size);
    switch (font_size) {
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

    bc_module_lcd_draw_string(x, y, text);
}
