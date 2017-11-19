#include <usb_talk.h>
#include <bc_scheduler.h>
#include <bc_usb_cdc.h>
#include <bc_radio_pub.h>
#include <base64.h>
#include <application.h>

#define USB_TALK_MAX_TOKENS 100

#define USB_TALK_TOKEN_ARRAY         0
#define USB_TALK_TOKEN_TOPIC         1
#define USB_TALK_TOKEN_PAYLOAD       2
#define USB_TALK_TOKEN_PAYLOAD_KEY   3
#define USB_TALK_TOKEN_PAYLOAD_VALUE 4

static struct
{
    char tx_buffer[512];
    char rx_buffer[1024];
    size_t rx_length;
    size_t tx_length;
    bool rx_error;

    const usb_talk_subscribe_t *subscribes;
    int subscribes_length;

#if TALK_OVER_CDC
#else
    uint8_t read_fifo_buffer[1024];
    bc_fifo_t read_fifo;
    uint8_t write_fifo_buffer[1024];
    bc_fifo_t write_fifo;
#endif

} _usb_talk;

#if TALK_OVER_CDC
static void _usb_talk_cdc_read_task(void *param);
#else
static void _usb_talk_uart_event_handler(bc_uart_channel_t channel, bc_uart_event_t event, void  *event_param);
#endif
static void _usb_talk_process_character(char character);
static void _usb_talk_process_message(char *message, size_t length);
static bool _usb_talk_token_get_int(const char *buffer, jsmntok_t *token, int *value);
static bool _usb_talk_token_get_float(const char *buffer, jsmntok_t *token, float *value);
static bool _usb_talk_token_get_string(const char *buffer, jsmntok_t *token, char *str, size_t *length);
static bool _usb_talk_payload_get_node_id(const char *buffer, jsmntok_t *token, uint64_t *value);

void usb_talk_init(void)
{
    memset(&_usb_talk, 0, sizeof(_usb_talk));

#if TALK_OVER_CDC
    bc_usb_cdc_init();
#else
    bc_fifo_init(&_usb_talk.read_fifo, _usb_talk.read_fifo_buffer, sizeof(_usb_talk.read_fifo_buffer));
    bc_fifo_init(&_usb_talk.write_fifo, _usb_talk.write_fifo_buffer, sizeof(_usb_talk.write_fifo_buffer));
    bc_uart_init(BC_UART_UART2, BC_UART_BAUDRATE_115200, BC_UART_SETTING_8N1);
    bc_uart_set_async_fifo(BC_UART_UART2, &_usb_talk.write_fifo, &_usb_talk.read_fifo);
#endif
}

void usb_talk_subscribes(const usb_talk_subscribe_t *subscribes, int length)
{
    _usb_talk.subscribes = subscribes;
    _usb_talk.subscribes_length = length;
    if ((subscribes != NULL) && length > 0)
    {
#if TALK_OVER_CDC
        bc_scheduler_register(_usb_talk_cdc_read_task, NULL, 0);
#else
        bc_uart_set_event_handler(BC_UART_UART2, _usb_talk_uart_event_handler, NULL);
        bc_uart_async_read_start(BC_UART_UART2, 1000000);
#endif
    }
}

void usb_talk_send_string(const char *buffer)
{
#if TALK_OVER_CDC
    bc_usb_cdc_write(buffer, strlen(buffer));
#else
    bc_uart_async_write(BC_UART_UART2, buffer, strlen(buffer));
#endif
}

void usb_talk_send_format(const char *format, ...)
{
    va_list ap;
    size_t length;

    va_start(ap, format);
    length = vsnprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer), format, ap);
    va_end(ap);

#if TALK_OVER_CDC
    bc_usb_cdc_write(_usb_talk.tx_buffer, length);
#else
    bc_uart_async_write(BC_UART_UART2, _usb_talk.tx_buffer, length);
#endif
}


void usb_talk_message_start(const char *topic, ...)
{
    va_list ap;

    va_start(ap, topic);

    strcpy(_usb_talk.tx_buffer, "[\"");

    _usb_talk.tx_length = 2 + vsnprintf(_usb_talk.tx_buffer + 2, sizeof(_usb_talk.tx_buffer) - 2, topic, ap);

    strcpy(_usb_talk.tx_buffer + _usb_talk.tx_length, "\", ");

    _usb_talk.tx_length += 3;

    va_end(ap);
}

void usb_talk_message_start_id(uint64_t *device_address, const char *topic, ...)
{
    va_list ap;

    strcpy(_usb_talk.tx_buffer, "[\"");

    _usb_talk.tx_length = 2 + snprintf(_usb_talk.tx_buffer + 2, sizeof(_usb_talk.tx_buffer) - 2, USB_TALK_DEVICE_ADDRESS, *device_address);

    _usb_talk.tx_buffer[_usb_talk.tx_length++] = '/';

    va_start(ap, topic);

    _usb_talk.tx_length += vsnprintf(_usb_talk.tx_buffer + _usb_talk.tx_length, sizeof(_usb_talk.tx_buffer) - _usb_talk.tx_length, topic, ap);

    strcpy(_usb_talk.tx_buffer + _usb_talk.tx_length, "\", ");

    _usb_talk.tx_length += 3;

    va_end(ap);
}

void usb_talk_message_append(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);

    _usb_talk.tx_length += vsnprintf(_usb_talk.tx_buffer + _usb_talk.tx_length, sizeof(_usb_talk.tx_buffer) - _usb_talk.tx_length, format, ap);

    va_end(ap);
}

void usb_talk_message_send(void)
{
    strcpy(_usb_talk.tx_buffer + _usb_talk.tx_length, "]\n");

    _usb_talk.tx_length += 2;

#if TALK_OVER_CDC
    bc_usb_cdc_write(_usb_talk.tx_buffer, _usb_talk.tx_length);
#else
    bc_uart_async_write(BC_UART_UART2, _usb_talk.tx_buffer, _usb_talk.tx_length);
#endif
}

void usb_talk_publish_null(uint64_t *device_address, const char *subtopics)
{
    usb_talk_send_format("[\"%012llx/%s\", null]\n", *device_address, subtopics);
}

void usb_talk_publish_bool(uint64_t *device_address, const char *subtopics, bool *value)
{
    if (value == NULL)
    {
        usb_talk_publish_null(device_address, subtopics);

        return;
    }

    usb_talk_send_format("[\"%012llx/%s\", %s]\n", *device_address, subtopics, *value ? "true" : "false");
}

void usb_talk_publish_int(uint64_t *device_address, const char *subtopics, int *value)
{
    if (value == NULL)
    {
        usb_talk_publish_null(device_address, subtopics);

        return;
    }

    usb_talk_send_format("[\"%012llx/%s\", %d]\n", *device_address, subtopics, *value);
}

void usb_talk_publish_float(uint64_t *device_address, const char *subtopics, float *value)
{
    if (value == NULL)
    {
        usb_talk_publish_null(device_address, subtopics);

        return;
    }

    usb_talk_send_format("[\"%012llx/%s\", %0.2f]\n", *device_address, subtopics, *value);
}

void usb_talk_publish_complex_bool(uint64_t *device_address, const char *subtopic, const char *number, const char *name, bool *state)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/%s/%s/%s\", %s]\n",
                *device_address, subtopic, number, name, *state ? "true" : "false");

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_event_count(uint64_t *device_address, const char *name, uint16_t *event_count)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
             "[\"%012llx/%s/event-count\", %" PRIu16 "]\n",
             *device_address, name, *event_count);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_led(uint64_t *device_address, bool *state)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/led/-/state\", %s]\n",
                *device_address, *state ? "true" : "false");

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_temperature(uint64_t *device_address, uint8_t channel, float *celsius)
{
    if(channel == BC_RADIO_PUB_CHANNEL_A)
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/thermometer/a/temperature\", %0.2f]\n",
                *device_address, *celsius);
    }
    else if (channel == BC_RADIO_PUB_CHANNEL_B)
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/thermometer/b/temperature\", %0.2f]\n",
                *device_address, *celsius);
    }
    else if (channel == BC_RADIO_PUB_CHANNEL_SET_POINT)
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                        "[\"%012llx/thermometer/set-point/temperature\", %0.2f]\n",
                        *device_address, *celsius);
    }
    else
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/thermometer/%d:%d/temperature\", %0.2f]\n",
                *device_address, ((channel & 0x80) >> 7), (channel & ~0x80), *celsius);
    }

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_humidity(uint64_t *device_address, uint8_t channel, float *relative_humidity)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/hygrometer/%d:%d/relative-humidity\", %0.1f]\n",
                *device_address, ((channel & 0x80) >> 7), (channel & ~0x80), *relative_humidity);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_lux_meter(uint64_t *device_address, uint8_t channel, float *illuminance)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/lux-meter/%d:%d/illuminance\", %0.1f]\n",
                *device_address,  ((channel & 0x80) >> 7), (channel & ~0x80), *illuminance);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_barometer(uint64_t *device_address, uint8_t channel, float *pressure, float *altitude)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/barometer/%d:%d/pressure\", %0.2f]\n",
                *device_address,  ((channel & 0x80) >> 7), (channel & ~0x80), *pressure);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);

    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/barometer/%d:%d/altitude\", %0.2f]\n",
                *device_address,  ((channel & 0x80) >> 7), (channel & ~0x80), *altitude);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_co2(uint64_t *device_address, float *concentration)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/co2-meter/-/concentration\", %.0f]\n",
                *device_address, *concentration);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_light(uint64_t *device_address, bool *state)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/light/-/state\", %s]\n",
                *device_address, *state ? "true" : "false");

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_relay(uint64_t *device_address, bool *state)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/relay/-/state\", %s]\n",
                *device_address, *state ? "true" : "false");

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_module_relay(uint64_t *device_address, uint8_t *number, bc_module_relay_state_t *state)
{
    if (*state == BC_MODULE_RELAY_STATE_UNKNOWN)
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                    "[\"%012llx/relay/0:%d/state\", null]\n",
                    *device_address, *number);
    }
    else
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                    "[\"%012llx/relay/0:%d/state\", %s]\n",
                    *device_address, *number, *state == BC_MODULE_RELAY_STATE_TRUE ? "true" : "false");
    }

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_encoder(uint64_t *device_address, int *increment)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
             "[\"%012llx/encoder/-/increment\", %d]\n",
             *device_address, *increment);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_flood_detector(uint64_t *device_address, const char *number, bool *state)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
             "[\"%012llx/flood-detector/%c/alarm\", %s]\n",
             *device_address, *number, *state ? "true" : "false");

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_accelerometer_acceleration(uint64_t *device_address, float *x_axis, float *y_axis, float *z_axis)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
            "[\"%012llx/accelerometer/-/acceleration\", [%0.2f,%0.2f,%0.2f]]\n",
            *device_address, *x_axis, *y_axis, *z_axis);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_nodes(uint64_t *peer_devices_address, int lenght)
{
    int offset = snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                 "[\"/nodes\", [" );
    bool empty = true;
    for (int i = 0; i < lenght; i++)
    {
        if (peer_devices_address[i] == 0)
        {
            continue;
        }

        offset += snprintf(_usb_talk.tx_buffer + offset, sizeof(_usb_talk.tx_buffer) - offset,
                empty ? "\"%012llx\"" : ",\"%012llx\"",
                peer_devices_address[i]);

        empty = false;
    }

    strncpy(_usb_talk.tx_buffer + offset, "]]\n", sizeof(_usb_talk.tx_buffer) - offset);

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

#if TALK_OVER_CDC
static void _usb_talk_cdc_read_task(void *param)
{
    (void) param;

    while (true)
    {
        static uint8_t buffer[16];

        size_t length = bc_usb_cdc_read(buffer, sizeof(buffer));

        if (length == 0)
        {
            break;
        }

        for (size_t i = 0; i < length; i++)
        {
            _usb_talk_process_character((char) buffer[i]);
        }
    }

    bc_scheduler_plan_current_now();
}
#else

static void _usb_talk_uart_event_handler(bc_uart_channel_t channel, bc_uart_event_t event, void  *event_param)
{
    (void) channel;
    (void) event_param;

    if (event == BC_UART_EVENT_ASYNC_READ_DATA)
    {
        while (true)
        {
            static uint8_t buffer[16];

            size_t length = bc_uart_async_read(BC_UART_UART2, buffer, sizeof(buffer));

            if (length == 0)
            {
                break;
            }

            for (size_t i = 0; i < length; i++)
            {
                _usb_talk_process_character((char) buffer[i]);
            }
        }
    }
}
#endif

static void _usb_talk_process_character(char character)
{
    if (character == '\n')
    {
        if (!_usb_talk.rx_error && _usb_talk.rx_length > 0)
        {
            _usb_talk_process_message(_usb_talk.rx_buffer, _usb_talk.rx_length);
        }

        _usb_talk.rx_length = 0;
        _usb_talk.rx_error = false;

        return;
    }

    if (_usb_talk.rx_length == sizeof(_usb_talk.rx_buffer))
    {
        _usb_talk.rx_error = true;
    }
    else
    {
        if (!_usb_talk.rx_error)
        {
            _usb_talk.rx_buffer[_usb_talk.rx_length++] = character;
        }
    }
}

static void _usb_talk_process_message(char *message, size_t length)
{
    static jsmn_parser parser;
    static jsmntok_t tokens[USB_TALK_MAX_TOKENS];

    jsmn_init(&parser);

    int token_count = jsmn_parse(&parser, (const char *) message, length, tokens, USB_TALK_MAX_TOKENS);

    if (token_count < 3)
    {
        return;
    }

    if (tokens[USB_TALK_TOKEN_ARRAY].type != JSMN_ARRAY || tokens[USB_TALK_TOKEN_ARRAY].size != 2)
    {
        return;
    }

    if (tokens[USB_TALK_TOKEN_TOPIC].type != JSMN_STRING || tokens[USB_TALK_TOKEN_TOPIC].size != 0)
    {
        return;
    }

    size_t topic_length = tokens[USB_TALK_TOKEN_TOPIC].end - tokens[USB_TALK_TOKEN_TOPIC].start;

    char *topic = message + tokens[USB_TALK_TOKEN_TOPIC].start;

    uint64_t device_address = 0;

    if ((topic[0] != '$') && (topic[0] != '/'))
    {
        if (topic_length < 14)
        {
            return;
        }
        if(topic[12] != '/')
        {
            return;
        }
        sscanf(topic, "%012llx/", &device_address);
        topic += 13;
        topic_length -= 13;
    }

    for (int i = 0; i < _usb_talk.subscribes_length; i++)
    {
        if (strncmp(_usb_talk.subscribes[i].topic, topic, topic_length) == 0)
        {
            usb_talk_payload_t payload = {
                    message,
                    token_count - USB_TALK_TOKEN_PAYLOAD,
                    tokens + USB_TALK_TOKEN_PAYLOAD
            };
            _usb_talk.subscribes[i].callback(&device_address, &payload, (usb_talk_subscribe_t *) &_usb_talk.subscribes[i]);
        }
    }
}

bool usb_talk_payload_get_bool(usb_talk_payload_t *payload, bool *value)
{
    if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[0], "true"))
    {
        *value = true;
        return true;
    }
    else if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[0], "false"))
    {
        *value = false;
        return true;
    }
    return false;
}

bool usb_talk_payload_get_key_bool(usb_talk_payload_t *payload, const char *key, bool *value)
{
    if (payload->tokens[0].type != JSMN_OBJECT)
    {
        return false;
    }

    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {
            if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i + 1], "true"))
            {
                *value = true;
                return true;
            }
            else if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i + 1], "false"))
            {
                *value = false;
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    return false;
}

bool usb_talk_payload_get_data(usb_talk_payload_t *payload, uint8_t *buffer, size_t *length)
{
    if (payload->tokens[0].type != JSMN_STRING)
    {
        return false;
    }

    uint32_t input_length = payload->tokens[0].end - payload->tokens[0].start;

    size_t data_length = base64_calculate_decode_length(&payload->buffer[payload->tokens[0].start], input_length);

    if (data_length > *length)
    {
        return false;
    }

    return base64_decode(&payload->buffer[payload->tokens[0].start], input_length, buffer, (uint32_t *)length);
}

bool usb_talk_payload_get_key_data(usb_talk_payload_t *payload, const char *key, uint8_t *buffer, size_t *length)
{
    if (payload->tokens[0].type != JSMN_OBJECT)
    {
        return false;
    }

    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {
            if (payload->tokens[i + 1].type != JSMN_STRING)
            {
                return false;
            }

            uint32_t input_length = payload->tokens[i + 1].end - payload->tokens[i + 1].start;

            size_t data_length = base64_calculate_decode_length(&payload->buffer[payload->tokens[i + 1].start], input_length);

            if (data_length > *length)
            {
                return false;
            }

            return base64_decode(&payload->buffer[payload->tokens[i + 1].start], input_length, buffer, (uint32_t *)length);
        }
    }
    return false;
}

bool usb_talk_payload_get_enum(usb_talk_payload_t *payload, int *value, ...)
{
    char temp[11];
    char *str;
    int j = 0;

    jsmntok_t *token = &payload->tokens[0];

    if (token->type != JSMN_STRING)
    {
        return false;
    }

    size_t length = token->end - token->start;

    if (length > (sizeof(temp) - 1))
    {
        return false;
    }

    memset(temp, 0x00, sizeof(temp));

    strncpy(temp, payload->buffer + token->start, length);

    va_list vl;
    va_start(vl, value);
    str = va_arg(vl, char*);
    while (str != NULL)
    {
        if (strcmp(str, temp) == 0)
        {
            *value = j;
            return true;
        }
        str = va_arg(vl, char*);
        j++;
    }
    va_end(vl);

    return false;
}

bool usb_talk_payload_get_key_enum(usb_talk_payload_t *payload, const char *key, int *value, ...)
{
    char temp[32];
    char *str;
    int j = 0;

    if (payload->tokens[0].type != JSMN_OBJECT)
    {
        return false;
    }

    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {

            jsmntok_t *token = &payload->tokens[i + 1];
            size_t length = token->end - token->start;

            if (length > (sizeof(temp) - 1))
            {
                return false;
            }

            memset(temp, 0x00, sizeof(temp));

            strncpy(temp, payload->buffer + token->start, length);

            va_list vl;
            va_start(vl, value);
            str = va_arg(vl, char*);
            while (str != NULL)
            {
                if (strcmp(str, temp) == 0)
                {
                    *value = j;
                    return true;
                }
                str = va_arg(vl, char*);
                j++;
            }
            va_end(vl);

            return false;
        }
    }
    return false;
}

bool usb_talk_payload_get_int(usb_talk_payload_t *payload, int *value)
{
    return _usb_talk_token_get_int(payload->buffer, &payload->tokens[0], value);
}

bool usb_talk_payload_get_key_int(usb_talk_payload_t *payload, const char *key, int *value)
{
    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {
            return _usb_talk_token_get_int(payload->buffer, &payload->tokens[i + 1], value);
        }
    }
    return false;
}

bool usb_talk_payload_get_float(usb_talk_payload_t *payload, float *value)
{
    return _usb_talk_token_get_float(payload->buffer, &payload->tokens[0], value);
}

bool usb_talk_payload_get_key_float(usb_talk_payload_t *payload, const char *key, float *value)
{
    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {
            return _usb_talk_token_get_float(payload->buffer, &payload->tokens[i + 1], value);
        }
    }
    return false;
}

bool usb_talk_payload_get_string(usb_talk_payload_t *payload, char *buffer, size_t *length)
{
    if (payload->tokens[0].type != JSMN_STRING)
    {
        return false;
    }
    uint32_t token_length = payload->tokens[0].end - payload->tokens[0].start;
    if (token_length > *length - 1)
    {
        return false;
    }
    strncpy(buffer, &payload->buffer[payload->tokens[0].start], token_length);
    *length = token_length;
    buffer[token_length] = 0;
    return true;
}

bool usb_talk_payload_get_key_string(usb_talk_payload_t *payload, const char *key, char *buffer, size_t *length)
{
    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {
            if (payload->tokens[i + 1].type != JSMN_STRING)
            {
                return false;
            }
            uint32_t token_length = payload->tokens[i + 1].end - payload->tokens[i + 1].start;
            if (token_length > *length - 1)
            {
                return false;
            }
            strncpy(buffer, &payload->buffer[payload->tokens[i + 1].start], token_length);
            *length = token_length;
            buffer[token_length] = 0;
            return true;
        }
    }
    return false;
}

bool usb_talk_payload_get_node_id(usb_talk_payload_t *payload, uint64_t *value)
{
    return _usb_talk_payload_get_node_id(payload->buffer, &payload->tokens[0], value);
}

bool usb_talk_payload_get_key_node_id(usb_talk_payload_t *payload, const char *key, uint64_t *value)
{
    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {
            if (payload->tokens[i + 1].type != JSMN_STRING)
            {
                return false;
            }

            return _usb_talk_payload_get_node_id(payload->buffer, &payload->tokens[i + 1], value);
        }
    }
    return false;
}

bool usb_talk_payload_get_compound_buffer(usb_talk_payload_t *payload, uint8_t *buffer, size_t *length, int *count_sum)
{
    if (payload->tokens[0].type != JSMN_ARRAY)
    {
        return false;
    }

    if (payload->tokens[0].size % 2 != 0)
    {
        return false;
    }

    int count;
    char str[12];
    size_t str_length;
    size_t _length = 0;
    int _count_sum = 0;

    for (int i = 1; (i + 1 < payload->token_count) && (_length + 5 <= *length); i += 2)
    {
        if (!_usb_talk_token_get_int(payload->buffer, &payload->tokens[i], &count))
        {
            return false;
        }

        if (_count_sum < *count_sum)
        {
            _count_sum += count;
            continue;
        }

        str_length = sizeof(str);

        if (!_usb_talk_token_get_string(payload->buffer, &payload->tokens[i + 1], str, &str_length))
        {
            return false;
        }

        if (((str_length != 7) && (str_length != 11)) || (str[0] != '#'))
        {
            return false;
        }

        _count_sum += count;

        *(buffer + _length++) = count;
        *(buffer + _length++) = usb_talk_hex_to_u8(str + 1);
        *(buffer + _length++) = usb_talk_hex_to_u8(str + 3);
        *(buffer + _length++) = usb_talk_hex_to_u8(str + 5);
        *(buffer + _length++) =  (str_length == 11) ?  usb_talk_hex_to_u8(str + 8) : 0x00;
    }

    *length = _length;
    *count_sum += _count_sum;

    return true;
}

bool usb_talk_is_string_token_equal(const char *buffer, jsmntok_t *token, const char *string)
{
    size_t token_length;

    token_length = (size_t) (token->end - token->start);

    if (strlen(string) != token_length)
    {
        return false;
    }

    if (strncmp(string, &buffer[token->start], token_length) != 0)
    {
        return false;
    }

    return true;
}

uint8_t usb_talk_hex_to_u8(const char *hex)
{
    uint8_t high = (*hex <= '9') ? *hex - '0' : toupper(*hex) - 'A' + 10;
    uint8_t low = (*(hex+1) <= '9') ? *(hex+1) - '0' : toupper(*(hex+1)) - 'A' + 10;
    return (high << 4) | low;
}

static bool _usb_talk_token_get_int(const char *buffer, jsmntok_t *token, int *value)
{
    if (token->type != JSMN_PRIMITIVE)
    {
        return false;
    }

    size_t length = (size_t) (token->end - token->start);

    char str[10 + 1];

    if (length >= sizeof(str))
    {
        return false;
    }

    memset(str, 0, sizeof(str));

    strncpy(str, buffer + token->start, length);

    if (strncmp(str, "null", sizeof(str)) == 0)
    {
        return USB_TALK_INT_VALUE_NULL;
    }

    if (strchr(str, 'e'))
    {
        *value = (int) strtof(str, NULL);
    }
    else
    {
        *value = (int) strtol(str, NULL, 10);
    }

    return true;
}

static bool _usb_talk_token_get_float(const char *buffer, jsmntok_t *token, float *value)
{
    if (token->type != JSMN_PRIMITIVE)
    {
        return false;
    }

    size_t length = (size_t) (token->end - token->start);

    char str[10 + 1];

    if (length >= sizeof(str))
    {
        return false;
    }

    memset(str, 0, sizeof(str));

    strncpy(str, buffer + token->start, length);

    *value = strtof(str, NULL);

    return true;
}

static bool _usb_talk_token_get_string(const char *buffer, jsmntok_t *token, char *str, size_t *length)
{
    if (token->type != JSMN_STRING)
    {
        return false;
    }
    uint32_t token_length = token->end - token->start;
    if (token_length > *length)
    {
        return false;
    }
    strncpy(str, &buffer[token->start], token_length);
    *length = token_length;
    return true;
}

static bool _usb_talk_payload_get_node_id(const char *buffer, jsmntok_t *token, uint64_t *value)
{
    if (token->type != JSMN_STRING)
    {
        return false;
    }

    uint32_t token_length = token->end - token->start;

    if (token_length != 12)
    {
        return false;
    }

    char str_id[13];

    strncpy(str_id, buffer + token->start, 12);

    str_id[12] = 0;

    sscanf(str_id, USB_TALK_DEVICE_ADDRESS, value);

    return true;
}

