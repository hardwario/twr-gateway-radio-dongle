#include <usb_talk.h>
#include <twr_scheduler.h>
#include <twr_usb_cdc.h>
#include <twr_radio_pub.h>
#include <twr_base64.h>
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

    usb_talk_subscribe_t subs[USB_TALK_SUB_LENGTH];
    int subs_length;
    char subs_topic[USB_TALK_SUB_LENGTH][USB_TALK_SUB_TOPIC_MAX_LENGTH];

    bool read_start;

#if TALK_OVER_CDC
#else
    uint8_t read_fifo_buffer[1024];
    twr_fifo_t read_fifo;
    uint8_t write_fifo_buffer[1024];
    twr_fifo_t write_fifo;
#endif

} _usb_talk;

#if TALK_OVER_CDC
static void _usb_talk_cdc_read_task(void *param);
#else
static void _usb_talk_uart_event_handler(twr_uart_channel_t channel, twr_uart_event_t event, void  *event_param);
#endif
static void _usb_talk_read_start(void);
static void _usb_talk_process_character(char character);
static void _usb_talk_process_message(char *message, size_t length);
static bool _usb_talk_token_get_int(const char *buffer, jsmntok_t *token, int *value);
static bool _usb_talk_token_get_float(const char *buffer, jsmntok_t *token, float *value);
static bool _usb_talk_token_get_string(const char *buffer, jsmntok_t *token, char *str, size_t *length);
static bool _usb_talk_payload_get_node_id(const char *buffer, jsmntok_t *token, uint64_t *value);
static bool _usb_talk_payload_get_color(const char *buffer, jsmntok_t *token, uint32_t *color);

void usb_talk_init(void)
{
    memset(&_usb_talk, 0, sizeof(_usb_talk));

#if TALK_OVER_CDC
    twr_usb_cdc_init();
#else
    twr_fifo_init(&_usb_talk.read_fifo, _usb_talk.read_fifo_buffer, sizeof(_usb_talk.read_fifo_buffer));
    twr_fifo_init(&_usb_talk.write_fifo, _usb_talk.write_fifo_buffer, sizeof(_usb_talk.write_fifo_buffer));
    twr_uart_init(TWR_UART_UART2, TWR_UART_BAUDRATE_115200, TWR_UART_SETTING_8N1);
    twr_uart_set_async_fifo(TWR_UART_UART2, &_usb_talk.write_fifo, &_usb_talk.read_fifo);
#endif
}

void usb_talk_subscribes(const usb_talk_subscribe_t *subscribes, int length)
{
    _usb_talk.subscribes = subscribes;

    _usb_talk.subscribes_length = subscribes != NULL ? length : 0;

    _usb_talk_read_start();
}

bool usb_talk_add_sub(const char *topic, usb_talk_sub_callback_t callback, uint8_t number, void *param)
{
    usb_talk_subscribe_t *sub = NULL;

    for (int i = 0; i < _usb_talk.subs_length; i++)
    {
        if (strncmp(_usb_talk.subs[i].topic, topic, USB_TALK_SUB_TOPIC_MAX_LENGTH) == 0)
        {
            sub = _usb_talk.subs + i;

            break;
        }
    }

    if (sub == NULL)
    {
        if (_usb_talk.subs_length >= USB_TALK_SUB_LENGTH)
        {
            return false;
        }

        sub = _usb_talk.subs + _usb_talk.subs_length;

        strncpy(_usb_talk.subs_topic[_usb_talk.subs_length], topic, USB_TALK_SUB_TOPIC_MAX_LENGTH);

        sub->topic = _usb_talk.subs_topic[_usb_talk.subs_length];

        _usb_talk.subs_length++;
    }

    sub->callback = callback;

    sub->number = number;

    sub->param = param;

    _usb_talk_read_start();

    return true;
}

void usb_talk_send_string(const char *buffer)
{
#if TALK_OVER_CDC
    twr_usb_cdc_write(buffer, strlen(buffer));
#else
    twr_uart_async_write(TWR_UART_UART2, buffer, strlen(buffer));
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
    twr_usb_cdc_write(_usb_talk.tx_buffer, length);
#else
    twr_uart_async_write(TWR_UART_UART2, _usb_talk.tx_buffer, length);
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

void usb_talk_message_append_float(const char *format, float *value)
{
    if (value == NULL)
    {
        strncpy(_usb_talk.tx_buffer + _usb_talk.tx_length, "null", sizeof(_usb_talk.tx_buffer) - _usb_talk.tx_length);

        _usb_talk.tx_length += 4;
    }
    else
    {
        _usb_talk.tx_length += snprintf(_usb_talk.tx_buffer + _usb_talk.tx_length, sizeof(_usb_talk.tx_buffer) - _usb_talk.tx_length, format, *value);
    }
}

void usb_talk_message_send(void)
{
    strcpy(_usb_talk.tx_buffer + _usb_talk.tx_length, "]\n");

    _usb_talk.tx_length += 2;

#if TALK_OVER_CDC
    twr_usb_cdc_write(_usb_talk.tx_buffer, _usb_talk.tx_length);
#else
    twr_uart_async_write(TWR_UART_UART2, _usb_talk.tx_buffer, _usb_talk.tx_length);
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
    if (event_count == NULL)
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                 "[\"%012llx/%s/event-count\", null]\n",
                 *device_address, name);
    }
    else
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                 "[\"%012llx/%s/event-count\", %" PRIu16 "]\n",
                 *device_address, name, *event_count);
    }
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
    if(channel == TWR_RADIO_PUB_CHANNEL_A)
    {
        usb_talk_publish_float(device_address, "thermometer/a/temperature", celsius);
        return;
    }
    else if (channel == TWR_RADIO_PUB_CHANNEL_B)
    {
        usb_talk_publish_float(device_address, "thermometer/b/temperature", celsius);
        return;
    }
    else if (channel == TWR_RADIO_PUB_CHANNEL_SET_POINT)
    {
        usb_talk_publish_float(device_address, "thermometer/set-point/temperature", celsius);
        return;
    }

    usb_talk_message_start_id(device_address, "thermometer/%d:%d/temperature", ((channel & 0x80) >> 7), (channel & ~0x80));

    usb_talk_message_append_float("%.2f", celsius);

    usb_talk_message_send();
}

void usb_talk_publish_humidity(uint64_t *device_address, uint8_t channel, float *relative_humidity)
{
    usb_talk_message_start_id(device_address, "hygrometer/%d:%d/relative-humidity", ((channel & 0x80) >> 7), (channel & ~0x80));

    usb_talk_message_append_float("%.1f", relative_humidity);

    usb_talk_message_send();
}

void usb_talk_publish_lux_meter(uint64_t *device_address, uint8_t channel, float *illuminance)
{
    usb_talk_message_start_id(device_address, "lux-meter/%d:%d/illuminance", ((channel & 0x80) >> 7), (channel & ~0x80));

    usb_talk_message_append_float("%.1f", illuminance);

    usb_talk_message_send();
}

void usb_talk_publish_barometer(uint64_t *device_address, uint8_t channel, float *pressure, float *altitude)
{
    usb_talk_message_start_id(device_address, "barometer/%d:%d/pressure", ((channel & 0x80) >> 7), (channel & ~0x80));

    usb_talk_message_append_float("%.2f", pressure);

    usb_talk_message_send();

    usb_talk_message_start_id(device_address, "barometer/%d:%d/altitude", ((channel & 0x80) >> 7), (channel & ~0x80));

    usb_talk_message_append_float("%.2f", altitude);

    usb_talk_message_send();
}

void usb_talk_publish_co2(uint64_t *device_address, float *concentration)
{
    usb_talk_message_start_id(device_address, "co2-meter/-/concentration");

    usb_talk_message_append_float("%.0f", concentration);

    usb_talk_message_send();
}

void usb_talk_publish_relay(uint64_t *device_address, bool *state)
{
    snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                "[\"%012llx/relay/-/state\", %s]\n",
                *device_address, *state ? "true" : "false");

    usb_talk_send_string((const char *) _usb_talk.tx_buffer);
}

void usb_talk_publish_module_relay(uint64_t *device_address, uint8_t *number, twr_module_relay_state_t *state)
{
    if (*state == TWR_MODULE_RELAY_STATE_UNKNOWN)
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                    "[\"%012llx/relay/0:%d/state\", null]\n",
                    *device_address, *number);
    }
    else
    {
        snprintf(_usb_talk.tx_buffer, sizeof(_usb_talk.tx_buffer),
                    "[\"%012llx/relay/0:%d/state\", %s]\n",
                    *device_address, *number, *state == TWR_MODULE_RELAY_STATE_TRUE ? "true" : "false");
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
    usb_talk_message_start_id(device_address, "accelerometer/-/acceleration");

    if (x_axis == NULL)
    {
        usb_talk_message_append("[null");
    }
    else
    {
        usb_talk_message_append("[%.6f", *x_axis);
    }

    if (y_axis == NULL)
    {
        usb_talk_message_append(", null");
    }
    else
    {
        usb_talk_message_append(", %.6f", *y_axis);
    }

    if (z_axis == NULL)
    {
        usb_talk_message_append(", null]");
    }
    else
    {
        usb_talk_message_append(", %.6f]", *z_axis);
    }

    usb_talk_message_send();
}

void usb_talk_publish_buffer(uint64_t *device_address, void *buffer, size_t length)
{
    usb_talk_message_start_id(device_address, "buffer/-/data");

    _usb_talk.tx_buffer[_usb_talk.tx_length++] = '[';

    for (size_t i = 0; i < length; i++)
    {
        usb_talk_message_append("%d, ", ((uint8_t *) buffer)[i]);
    }

    _usb_talk.tx_length--;

    _usb_talk.tx_buffer[_usb_talk.tx_length - 1] = ']';

    usb_talk_message_send();
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

        size_t length = twr_usb_cdc_read(buffer, sizeof(buffer));

        if (length == 0)
        {
            break;
        }

        for (size_t i = 0; i < length; i++)
        {
            _usb_talk_process_character((char) buffer[i]);
        }
    }

    twr_scheduler_plan_current_now();
}
#else

static void _usb_talk_uart_event_handler(twr_uart_channel_t channel, twr_uart_event_t event, void  *event_param)
{
    (void) channel;
    (void) event_param;

    if (event == TWR_UART_EVENT_ASYNC_READ_DATA)
    {
        while (true)
        {
            static uint8_t buffer[16];

            size_t length = twr_uart_async_read(TWR_UART_UART2, buffer, sizeof(buffer));

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

void _usb_talk_read_start(void)
{
    if (_usb_talk.read_start)
    {
        return;
    }

    if (((_usb_talk.subscribes != NULL) && (_usb_talk.subscribes_length > 0)) || ((_usb_talk.subs != NULL) && (_usb_talk.subs_length > 0)))
    {
#if TALK_OVER_CDC
        twr_scheduler_register(_usb_talk_cdc_read_task, NULL, 0);
#else
        twr_uart_set_event_handler(TWR_UART_UART2, _usb_talk_uart_event_handler, NULL);
        twr_uart_async_read_start(TWR_UART_UART2, 1000000);
#endif

        _usb_talk.read_start = true;
    }
}

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

    for (int i = 0; i < _usb_talk.subs_length; i++)
    {
        if (strncmp(_usb_talk.subs[i].topic, topic, topic_length) == 0)
        {
            usb_talk_payload_t payload = {
                    message,
                    token_count - USB_TALK_TOKEN_PAYLOAD,
                    tokens + USB_TALK_TOKEN_PAYLOAD
            };
            _usb_talk.subs[i].callback(&device_address, &payload, (usb_talk_subscribe_t *) &_usb_talk.subs[i]);
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

    size_t data_length = twr_base64_calculate_decode_length(&payload->buffer[payload->tokens[0].start], input_length);

    if (data_length > *length)
    {
        return false;
    }

    return twr_base64_decode( buffer, length, &payload->buffer[payload->tokens[0].start], input_length);
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

            size_t data_length = twr_base64_calculate_decode_length(&payload->buffer[payload->tokens[i + 1].start], input_length);

            if (data_length > *length)
            {
                return false;
            }

            return twr_base64_decode(buffer, length, &payload->buffer[payload->tokens[i + 1].start], input_length);
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
            return _usb_talk_payload_get_node_id(payload->buffer, &payload->tokens[i + 1], value);
        }
    }
    return false;
}

bool usb_talk_payload_get_color(usb_talk_payload_t *payload, uint32_t *color)
{
    return _usb_talk_payload_get_color(payload->buffer, &payload->tokens[0], color);
}

bool usb_talk_payload_get_key_color(usb_talk_payload_t *payload, const char *key, uint32_t *color)
{
    for (int i = 1; i + 1 < payload->token_count; i += 2)
    {
        if (usb_talk_is_string_token_equal(payload->buffer, &payload->tokens[i], key))
        {
            return _usb_talk_payload_get_color(payload->buffer, &payload->tokens[i + 1], color);
        }
    }
    return false;
}

bool usb_talk_payload_get_compound(usb_talk_payload_t *payload, uint8_t *compound, size_t *length, int *count_sum)
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
    size_t _length = 0;
    *count_sum = 0;

    for (int i = 1; (i + 1 < payload->token_count) && (_length + 5 <= *length); i += 2)
    {
        if (!_usb_talk_token_get_int(payload->buffer, &payload->tokens[i], &count))
        {
            return false;
        }

        if (count > 255)
        {
            count = 255;
        }

        compound[_length++] = count;
        *count_sum += count;

        if (!_usb_talk_payload_get_color(payload->buffer, &payload->tokens[i + 1], (uint32_t *) (compound + _length)))
        {
            return false;
        }

        _length += 4;
    }

    *length = _length;

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

static uint8_t _usb_talk_hex_to_u8(const char *hex)
{
    uint8_t high = (*hex <= '9') ? *hex - '0' : toupper(*hex) - 'A' + 10;
    uint8_t low = (*(hex+1) <= '9') ? *(hex+1) - '0' : toupper(*(hex+1)) - 'A' + 10;
    return (high << 4) | low;
}

static bool _usb_talk_payload_get_color(const char *buffer, jsmntok_t *token, uint32_t *color)
{
    if (token->type != JSMN_STRING)
    {
        return false;
    }

    char str[13];;
    size_t length = sizeof(str);

    if (!_usb_talk_token_get_string(buffer, token, str, &length))
    {
        return false;
    }

    if (((length != 7) && (length != 11)) || (str[0] != '#'))
    {
        return false;
    }

    uint8_t *pcolor = (uint8_t *) color;

    pcolor[3] = _usb_talk_hex_to_u8(str + 1);
    pcolor[2] =  _usb_talk_hex_to_u8(str + 3);
    pcolor[1] =  _usb_talk_hex_to_u8(str + 5);
    pcolor[0] =  (length == 11) ? _usb_talk_hex_to_u8(str + 8) : 0x00;

    return true;
}
