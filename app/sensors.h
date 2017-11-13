#ifndef _SENSOR_H
#define _SENSOR_H

#include <bc_common.h>
#include <bcl.h>

#define TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define TEMPERATURE_TAG_PUB_VALUE_CHANGE 0.1f
#define TEMPERATURE_TAG_UPDATE_INTERVAL (1 * 1000)

#define HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define HUMIDITY_TAG_PUB_VALUE_CHANGE 1.0f
#define HUMIDITY_TAG_UPDATE_INTERVAL (1 * 1000)

#define LUX_METER_TAG_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define LUX_METER_TAG_PUB_VALUE_CHANGE 5.0f
#define LUX_METER_TAG_UPDATE_INTERVAL (1 * 1000)

#define BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define BAROMETER_TAG_PUB_VALUE_CHANGE 10.0f
#define BAROMETER_TAG_UPDATE_INTERVAL (1 * 1000)

#define CO2_PUB_NO_CHANGE_INTERVAL (5 * 60 * 1000)
#define CO2_PUB_VALUE_CHANGE 50.0f
#define CO2_UPDATE_INTERVAL (15 * 1000)

typedef struct
{
    uint8_t channel;
    float value;
    bc_tick_t next_pub;

} event_param_t;

typedef struct
{
    bc_tag_temperature_t self;
    event_param_t param;

} temperature_tag_t;

typedef struct
{
    bc_tag_humidity_t self;
    event_param_t param;

} humidity_tag_t;

typedef struct
{
    bc_tag_lux_meter_t self;
    event_param_t param;

} lux_meter_tag_t;

typedef struct
{
    bc_tag_barometer_t self;
    event_param_t param;

} barometer_tag_t;

void sensors_init_all(uint64_t *my_device_address);

void temperature_tag_init(bc_i2c_channel_t i2c_channel, bc_tag_temperature_i2c_address_t i2c_address, temperature_tag_t *tag);

void humidity_tag_init(bc_tag_humidity_revision_t revision, bc_i2c_channel_t i2c_channel, humidity_tag_t *tag);

void lux_meter_tag_init(bc_i2c_channel_t i2c_channel, bc_tag_lux_meter_i2c_address_t i2c_address, lux_meter_tag_t *tag);

void barometer_tag_init(bc_i2c_channel_t i2c_channel, barometer_tag_t *tag);

void co2_module_init(void);

void pir_module_init(void);

#endif
