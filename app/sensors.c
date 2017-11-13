#include <sensors.h>
#include <usb_talk.h>
#include <bc_radio_pub.h>

static uint64_t *_device_address;

void sensors_init_all(uint64_t *my_device_address)
{
	_device_address = my_device_address;

	static temperature_tag_t temperature_tag_0_0;
	temperature_tag_init(BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT, &temperature_tag_0_0);

	static temperature_tag_t temperature_tag_0_1;
	temperature_tag_init(BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE, &temperature_tag_0_1);

	static temperature_tag_t temperature_tag_1_0;
	temperature_tag_init(BC_I2C_I2C1, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT, &temperature_tag_1_0);

	static temperature_tag_t temperature_tag_1_1;
	temperature_tag_init(BC_I2C_I2C1, BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE, &temperature_tag_1_1);

	//----------------------------

	static humidity_tag_t humidity_tag_0_0;
	humidity_tag_init(BC_TAG_HUMIDITY_REVISION_R1, BC_I2C_I2C0, &humidity_tag_0_0);

	static humidity_tag_t humidity_tag_0_2;
	humidity_tag_init(BC_TAG_HUMIDITY_REVISION_R2, BC_I2C_I2C0, &humidity_tag_0_2);

	static humidity_tag_t humidity_tag_0_4;
	humidity_tag_init(BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C0, &humidity_tag_0_4);

	static humidity_tag_t humidity_tag_1_0;
	humidity_tag_init(BC_TAG_HUMIDITY_REVISION_R1, BC_I2C_I2C1, &humidity_tag_1_0);

	static humidity_tag_t humidity_tag_1_2;
	humidity_tag_init(BC_TAG_HUMIDITY_REVISION_R2, BC_I2C_I2C1, &humidity_tag_1_2);

	static humidity_tag_t humidity_tag_1_4;
	humidity_tag_init(BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C1, &humidity_tag_1_4);

	//----------------------------

	static lux_meter_tag_t lux_meter_0_0;
	lux_meter_tag_init(BC_I2C_I2C0, BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT, &lux_meter_0_0);

	static lux_meter_tag_t lux_meter_0_1;
	lux_meter_tag_init(BC_I2C_I2C0, BC_TAG_LUX_METER_I2C_ADDRESS_ALTERNATE, &lux_meter_0_1);

	static lux_meter_tag_t lux_meter_1_0;
	lux_meter_tag_init(BC_I2C_I2C1, BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT, &lux_meter_1_0);

	static lux_meter_tag_t lux_meter_1_1;
	lux_meter_tag_init(BC_I2C_I2C1, BC_TAG_LUX_METER_I2C_ADDRESS_ALTERNATE, &lux_meter_1_1);

	//----------------------------

	static barometer_tag_t barometer_tag_0_0;
	barometer_tag_init(BC_I2C_I2C0, &barometer_tag_0_0);

	static barometer_tag_t barometer_tag_1_0;
	barometer_tag_init(BC_I2C_I2C1, &barometer_tag_1_0);

	//----------------------------

	co2_module_init();

	pir_module_init();
}

static void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_temperature_get_temperature_celsius(self, &value))
    {
        if ((fabs(value - param->value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
        	usb_talk_publish_temperature(_device_address, param->channel, &value);

            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL;
        }
    }
}

void temperature_tag_init(bc_i2c_channel_t i2c_channel, bc_tag_temperature_i2c_address_t i2c_address, temperature_tag_t *tag)
{
    memset(tag, 0, sizeof(*tag));

    tag->param.channel = i2c_address == BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT ? BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT: BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;

    bc_tag_temperature_init(&tag->self, i2c_channel, i2c_address);

    bc_tag_temperature_set_update_interval(&tag->self, TEMPERATURE_TAG_UPDATE_INTERVAL);

    bc_tag_temperature_set_event_handler(&tag->self, temperature_tag_event_handler, &tag->param);
}

static void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_humidity_get_humidity_percentage(self, &value))
    {
        if ((fabs(value - param->value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
        	 usb_talk_publish_humidity(_device_address, param->channel, &value);

            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL;
        }
    }
}

void humidity_tag_init(bc_tag_humidity_revision_t revision, bc_i2c_channel_t i2c_channel, humidity_tag_t *tag)
{
    memset(tag, 0, sizeof(*tag));

    if (revision == BC_TAG_HUMIDITY_REVISION_R1)
    {
        tag->param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    }
    else if (revision == BC_TAG_HUMIDITY_REVISION_R2)
    {
        tag->param.channel = BC_RADIO_PUB_CHANNEL_R2_I2C0_ADDRESS_DEFAULT;
    }
    else if (revision == BC_TAG_HUMIDITY_REVISION_R3)
    {
        tag->param.channel = BC_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT;
    }
    else
    {
        return;
    }

    if (i2c_channel == BC_I2C_I2C1)
    {
        tag->param.channel |= 0x80;
    }

    bc_tag_humidity_init(&tag->self, revision, i2c_channel, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);

    bc_tag_humidity_set_update_interval(&tag->self, HUMIDITY_TAG_UPDATE_INTERVAL);

    bc_tag_humidity_set_event_handler(&tag->self, humidity_tag_event_handler, &tag->param);
}

static void lux_meter_event_handler(bc_tag_lux_meter_t *self, bc_tag_lux_meter_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_LUX_METER_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_lux_meter_get_illuminance_lux(self, &value))
    {
        if ((fabs(value - param->value) >= LUX_METER_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
        	 usb_talk_publish_lux_meter(_device_address, param->channel, &value);

            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + LUX_METER_TAG_PUB_NO_CHANGE_INTEVAL;
        }
    }
}

void lux_meter_tag_init(bc_i2c_channel_t i2c_channel, bc_tag_lux_meter_i2c_address_t i2c_address, lux_meter_tag_t *tag)
{
    memset(tag, 0, sizeof(*tag));

    tag->param.channel = i2c_address == BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT ? BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT: BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;

    bc_tag_lux_meter_init(&tag->self, i2c_channel, i2c_address);

    bc_tag_lux_meter_set_update_interval(&tag->self, LUX_METER_TAG_UPDATE_INTERVAL);

    bc_tag_lux_meter_set_event_handler(&tag->self, lux_meter_event_handler, &tag->param);
}

static void barometer_tag_event_handler(bc_tag_barometer_t *self, bc_tag_barometer_event_t event, void *event_param)
{
    float pascal;
    float meter;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_BAROMETER_EVENT_UPDATE)
    {
        return;
    }

    if (!bc_tag_barometer_get_pressure_pascal(self, &pascal))
    {
        return;
    }

    if ((fabs(pascal - param->value) >= BAROMETER_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
    {
        if (!bc_tag_barometer_get_altitude_meter(self, &meter))
        {
            return;
        }

        usb_talk_publish_barometer(_device_address, param->channel, &pascal, &meter);

        param->value = pascal;
        param->next_pub = bc_scheduler_get_spin_tick() + BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL;
    }
}

void barometer_tag_init(bc_i2c_channel_t i2c_channel, barometer_tag_t *tag)
{
    memset(tag, 0, sizeof(*tag));

    tag->param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;

    bc_tag_barometer_init(&tag->self, i2c_channel);

    bc_tag_barometer_set_update_interval(&tag->self, BAROMETER_TAG_UPDATE_INTERVAL);

    bc_tag_barometer_set_event_handler(&tag->self, barometer_tag_event_handler, &tag->param);
}

void co2_event_handler(bc_module_co2_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    float value;

    if (event == BC_MODULE_CO2_EVENT_UPDATE)
    {
        if (bc_module_co2_get_concentration_ppm(&value))
        {
            if ((fabs(value - param->value) >= CO2_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                usb_talk_publish_co2(_device_address, &value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + CO2_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void co2_module_init(void)
{
    static event_param_t event_param = { .next_pub = 0 };
    bc_module_co2_init();
    bc_module_co2_set_update_interval(CO2_UPDATE_INTERVAL);
    bc_module_co2_set_event_handler(co2_event_handler, &event_param);
}

static void pir_event_handler(bc_module_pir_t *self, bc_module_pir_event_t event, void*event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_MODULE_PIR_EVENT_MOTION)
    {
        static uint16_t event_count = 0;
        event_count++;
        usb_talk_publish_event_count(_device_address, "pir", &event_count);
    }
}

void pir_module_init(void)
{
    static bc_module_pir_t pir;
    bc_module_pir_init(&pir);
    bc_module_pir_set_event_handler(&pir, pir_event_handler, NULL);
}

