#include <application.h>
#include <usb_talk.h>
#include <bc_device_id.h>
#include <radio.h>

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
static void lcd_button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
static void radio_event_handler(bc_radio_event_t event, void *event_param);
static void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param);
static void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param);
static void lux_meter_event_handler(bc_tag_lux_meter_t *self, bc_tag_lux_meter_event_t event, void *event_param);
static void barometer_tag_event_handler(bc_tag_barometer_t *self, bc_tag_barometer_event_t event, void *event_param);
static void co2_event_handler(bc_module_co2_event_t event, void *event_param);
static void pir_event_handler(bc_module_pir_t *self, bc_module_pir_event_t event, void*event_param);

static void led_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void led_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void module_relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void module_relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void lcd_text_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void lcd_screen_clear(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void led_strip_color_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void led_strip_brightness_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void led_strip_compound_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void radio_nodes_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void radio_node_add(uint64_t *device_address, usb_talk_payload_t *payload, void *param);
static void radio_node_remove(uint64_t *device_address, usb_talk_payload_t *payload, void *param);

static void _radio_node(usb_talk_payload_t *payload, bool (* call)(uint64_t));
static void _radio_pub_state_set(uint8_t type, uint64_t *device_address, bool state);
static void _radio_pub_state_get(uint8_t type, uint64_t *device_address);

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

    static bc_button_t lcd_left;
    bc_button_init_virtual(&lcd_left, BC_MODULE_LCD_BUTTON_LEFT, bc_module_lcd_get_button_driver(), false);
    bc_button_set_event_handler(&lcd_left, lcd_button_event_handler, NULL);

    static bc_button_t lcd_right;
    bc_button_init_virtual(&lcd_right, BC_MODULE_LCD_BUTTON_RIGHT, bc_module_lcd_get_button_driver(), false);
    bc_button_set_event_handler(&lcd_right, lcd_button_event_handler, NULL);

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

	static bc_tag_humidity_t humidity_tag_r3_0_40;
	bc_tag_humidity_init(&humidity_tag_r3_0_40, BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
	bc_tag_humidity_set_update_interval(&humidity_tag_r3_0_40, UPDATE_INTERVAL);
	static uint8_t humidity_tag_r3_0_40_i2c = (BC_I2C_I2C0 << 7) | 0x40 | 0x0f; // 0x0f - hack
	bc_tag_humidity_set_event_handler(&humidity_tag_r3_0_40, humidity_tag_event_handler, &humidity_tag_r3_0_40_i2c);

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

    static bc_tag_humidity_t humidity_tag_r3_1_40;
    bc_tag_humidity_init(&humidity_tag_r3_1_40, BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C1, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity_tag_r3_1_40, UPDATE_INTERVAL);
    static uint8_t humidity_tag_r3_1_40_i2c = (BC_I2C_I2C1 << 7) | 0x40 | 0x0f; // 0x0f - hack
    bc_tag_humidity_set_event_handler(&humidity_tag_r3_1_40, humidity_tag_event_handler, &humidity_tag_r3_1_40_i2c);

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

    //----------------------------

    bc_module_relay_init(&relay_0_0, BC_MODULE_RELAY_I2C_ADDRESS_DEFAULT);
    bc_module_relay_init(&relay_0_1, BC_MODULE_RELAY_I2C_ADDRESS_ALTERNATE);

    //----------------------------

    static bc_module_pir_t pir;
	bc_module_pir_init(&pir);
	bc_module_pir_set_event_handler(&pir, pir_event_handler, NULL);

    usb_talk_sub("/led/-/state/set", led_state_set, NULL);
    usb_talk_sub("/led/-/state/get", led_state_get, NULL);
    usb_talk_sub("/relay/-/state/set", relay_state_set, NULL);
    usb_talk_sub("/relay/-/state/get", relay_state_get, NULL);
    static uint8_t relay_0_number = 0;
    usb_talk_sub("/relay/0:0/state/set", module_relay_state_set, &relay_0_number);
    usb_talk_sub("/relay/0:0/state/get", module_relay_state_get, &relay_0_number);
    static uint8_t relay_1_number = 1;
    usb_talk_sub("/relay/0:1/state/set", module_relay_state_set, &relay_1_number);
    usb_talk_sub("/relay/0:1/state/get", module_relay_state_get, &relay_1_number);
    usb_talk_sub("/lcd/-/text/set", lcd_text_set, NULL);
    usb_talk_sub("/lcd/-/screen/clear", lcd_screen_clear, NULL);

    usb_talk_sub("/led-strip/-/color/set", led_strip_color_set, NULL);
    usb_talk_sub("/led-strip/-/brightness/set", led_strip_brightness_set, NULL);
    usb_talk_sub("/led-strip/-/compound/set", led_strip_compound_set, NULL);

    usb_talk_sub("/radio/-/nodes/get", radio_nodes_get, NULL);
    usb_talk_sub("/radio/-/node/add", radio_node_add, NULL);
    usb_talk_sub("/radio/-/node/remove", radio_node_remove, NULL);

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
        usb_talk_publish_push_button(&my_device_address, "-", &event_count);
        event_count++;
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        bc_radio_enrollment_start();
        bc_led_set_mode(&led, BC_LED_MODE_BLINK_FAST);
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
		usb_talk_publish_push_button(&my_device_address, "lcd:left", &event_left_count);
		event_left_count ++;
	}
	else
	{
		static uint16_t event_right_count = 0;
		usb_talk_publish_push_button(&my_device_address, "lcd:right", &event_right_count);
		event_right_count++;
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
    usb_talk_publish_push_button(peer_device_address, "-", event_count);
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
    if (*length == 2)
    {
    	switch (buffer[0]) {
            case RADIO_LED:
			{
				bool state = buffer[1];
				usb_talk_publish_led(peer_device_address, &state);
				break;
			}
			case RADIO_RELAY_0:
			{
				uint8_t number = 0;
				bc_module_relay_state_t state = buffer[1];
				usb_talk_publish_module_relay(peer_device_address, &number, &state);
				break;
			}
			case RADIO_RELAY_1:
			{
				uint8_t number = 1;
				bc_module_relay_state_t state = buffer[1];
				usb_talk_publish_module_relay(peer_device_address, &number, &state);
				break;
			}
			case RADIO_RELAY_POWER:
			{
				bool state = buffer[1];
				usb_talk_publish_relay(peer_device_address, &state);
				break;
			}
			default:
            {
				break;
            }
		}
    }
    else if (*length == 3)
    {
    	switch (buffer[0]) {
			case RADIO_PIR:
			{
				uint16_t event_count;
				memcpy(&event_count, buffer + 1 , sizeof(event_count));
				usb_talk_publish_event_count(peer_device_address, "pir", &event_count);
				break;
			}
			case RADIO_FLOOD_DETECTOR:
			{
				usb_talk_publish_flood_detector(peer_device_address, (char *)(buffer + 1), (bool *)(buffer + 2));
				break;
			}
			case RADIO_LCD_BUTTON_LEFT:
			{
				uint16_t event_count;
				memcpy(&event_count, buffer + 1 , sizeof(event_count));
				usb_talk_publish_push_button(peer_device_address, "lcd:left", &event_count);
				break;
			}
			case RADIO_LCD_BUTTON_RIGHT:
			{
				uint16_t event_count;
				memcpy(&event_count, buffer + 1 , sizeof(event_count));
				usb_talk_publish_push_button(peer_device_address, "lcd:right", &event_count);
				break;
			}
			case RADIO_ACCELEROMETER:
			{
				uint16_t event_count;
				memcpy(&event_count, buffer + 1 , sizeof(event_count));
				usb_talk_publish_push_button(peer_device_address, "accelerometer", &event_count);
				break;
			}
    	    default:
            {
			    break;
            }
    	}
    }
    else if (*length == 5)
    {
    	switch (buffer[0]) {
			case RADIO_THERMOSTAT_SET_POINT_TEMPERATURE:
			{
				float temperature;
				memcpy(&temperature, buffer + 1 , sizeof(temperature));
				usb_talk_publish_float(peer_device_address, "thermostat/set-point/temperature", &temperature);
				break;
			}
			default:
			{
				break;
			}

    	}
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

static void pir_event_handler(bc_module_pir_t *self, bc_module_pir_event_t event, void*event_param)
{
	(void) self;
	(void) event_param;

	if (event == BC_MODULE_PIR_EVENT_MOTION)
	{
		static uint16_t event_count = 0;
		event_count++;
		usb_talk_publish_event_count(&my_device_address, "pir", &event_count);
	}
}

static void led_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) param;
    bool state;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    if (my_device_address == *device_address)
    {
    	led_state = state;

        bc_led_set_mode(&led, led_state ? BC_LED_MODE_ON : BC_LED_MODE_OFF);

        usb_talk_publish_led(&my_device_address, &led_state);
    }
    else
    {
    	_radio_pub_state_set(RADIO_LED_SET, device_address, state);
    }
}

static void led_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;
    (void) param;

    if (my_device_address == *device_address)
	{
    	usb_talk_publish_led(&my_device_address, &led_state);
	}
    else
    {
    	_radio_pub_state_get(RADIO_LED_GET, device_address);
    }
}

static void relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) param;
    bool state;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    if (my_device_address == *device_address)
	{
        bc_module_power_relay_set_state(state);

        usb_talk_publish_relay(&my_device_address, &state);
	}
    else
    {
    	_radio_pub_state_set(RADIO_RELAY_POWER_SET, device_address, state);
    }
}

static void relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;
    (void) param;

    if (my_device_address == *device_address)
	{
        bool state = bc_module_power_relay_get_state();

        usb_talk_publish_relay(&my_device_address, &state);
	}
    else
    {
    	_radio_pub_state_get(RADIO_RELAY_POWER_GET, device_address);
    }

}

static void module_relay_state_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;

    bool state;
    uint8_t *number = (uint8_t *)param;

    if (!usb_talk_payload_get_bool(payload, &state))
    {
        return;
    }

    if (my_device_address == *device_address)
	{
    	bc_module_relay_set_state(*number == 0 ? &relay_0_0 : &relay_0_1, state);
    	bc_module_relay_state_t relay_state = state ? BC_MODULE_RELAY_STATE_TRUE : BC_MODULE_RELAY_STATE_FALSE;
    	usb_talk_publish_module_relay(&my_device_address, number, &relay_state);
	}
    else
    {
    	_radio_pub_state_set(*number == 0 ? RADIO_RELAY_0_SET : RADIO_RELAY_1_SET, device_address, state);
    }
}

static void module_relay_state_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) payload;
    uint8_t *number = (uint8_t *)param;

    if (my_device_address == *device_address)
	{

        bc_module_relay_state_t state = bc_module_relay_get_state(*number == 0 ? &relay_0_0 : &relay_0_1);

        usb_talk_publish_module_relay(&my_device_address, number, &state);
	}
    else
    {
    	_radio_pub_state_get(*number == 0 ? RADIO_RELAY_0_GET : RADIO_RELAY_1_GET, device_address);
    }

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

static void lcd_screen_clear(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) param;

    bool state;

    if (!usb_talk_payload_get_bool(payload, &state) || !state)
    {
        return;
    }

    if (my_device_address != *device_address)
	{
		return;
	}

    bc_module_lcd_clear();
}

static void led_strip_color_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
	(void) param;
	uint8_t buffer[1 + sizeof(uint64_t) + 4];
	buffer[0] = RADIO_LED_STRIP_COLOR_SET;
	memcpy(buffer + 1, device_address, sizeof(uint64_t));

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
	buffer[sizeof(uint64_t) + 4] =  (length == 11) ?  usb_talk_hex_to_u8(str + 8) : 0x00;

	bc_radio_pub_buffer(buffer, sizeof(buffer));
}

static void led_strip_brightness_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
	(void) param;
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
	memcpy(buffer + 1, device_address, sizeof(uint64_t));
	buffer[sizeof(uint64_t) + 1] = value;

	bc_radio_pub_buffer(buffer, sizeof(buffer));
}

static void led_strip_compound_set(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
	(void) param;
	uint8_t buffer[64 - 9];
	buffer[0] = RADIO_LED_STRIP_COMPOUND_SET;
	memcpy(buffer + 1, device_address, sizeof(uint64_t));

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

static void radio_nodes_get(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
    (void) param;
    (void) payload;

    if (my_device_address != *device_address)
	{
		return;
	}

    uint64_t address[BC_RADIO_MAX_DEVICES];

    bc_radio_get_peer_devices_address(address, BC_RADIO_MAX_DEVICES);

    usb_talk_publish_radio_nodes(&my_device_address, address, BC_RADIO_MAX_DEVICES);

}

static void radio_node_add(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
	(void) param;
    if (my_device_address != *device_address)
	{
		return;
	}

    _radio_node(payload, bc_radio_peer_device_add);
}

static void radio_node_remove(uint64_t *device_address, usb_talk_payload_t *payload, void *param)
{
	(void) param;
    if (my_device_address != *device_address)
	{
		return;
	}

    _radio_node(payload, bc_radio_peer_device_remove);
}

static void _radio_node(usb_talk_payload_t *payload, bool (* call)(uint64_t))
{
	char tmp[13];
	size_t length = sizeof(tmp);

	if (!usb_talk_payload_get_string(payload, tmp, &length))
	{
		return;
	}

	if (length == 12)
	{
		uint64_t device_address = 0;

		if (sscanf(tmp, "%012llx/", &device_address))
		{
			call(device_address);
		}
	}
}

static void _radio_pub_state_set(uint8_t type, uint64_t *device_address, bool state)
{
	uint8_t buffer[1 + sizeof(uint64_t) + 1];

	buffer[0] = type;
	memcpy(buffer + 1, device_address, sizeof(uint64_t));
	buffer[sizeof(uint64_t) + 1] = state;

	bc_radio_pub_buffer(buffer, sizeof(buffer));
}

static void _radio_pub_state_get(uint8_t type, uint64_t *device_address)
{
	uint8_t buffer[1 + sizeof(uint64_t)];

	buffer[0] = type;
	memcpy(buffer + 1, device_address, sizeof(uint64_t));

	bc_radio_pub_buffer(buffer, sizeof(buffer));
}
