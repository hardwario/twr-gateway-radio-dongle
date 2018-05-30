#include <eeprom.h>
#include <usb_talk.h>
#include <bc_radio.h>

static struct
{
    uint8_t alias_length;

    uint8_t buffer[EEPROM_ALIAS_ROW_LENGTH + 1];

} _eeprom;

static int _eeprom_alias_find_position_for_id(uint64_t *id);
static bool _eeprom_alias_length_set(uint8_t length);

void eeprom_init(void)
{
    memset(&_eeprom, 0, sizeof(_eeprom));

    uint8_t buffer[2];

    bc_eeprom_read(EEPROM_ALIAS_ADDRESS_LENGTH, buffer, sizeof(buffer));

    uint8_t neg = ~buffer[1];

    if (buffer[0] == neg)
    {
        _eeprom.alias_length = buffer[0];
    }

    // Remove alias for unknown nodes
    uint32_t address = EEPROM_ALIAS_ADDRESS_START;

    uint64_t tmp_id;

    uint64_t id_on_remove;

    while (true)
    {
        id_on_remove = 0;

        for (int i = 0; i < _eeprom.alias_length; i++)
        {
            bc_eeprom_read(address, &tmp_id, sizeof(tmp_id));

            if (!bc_radio_is_peer_device(tmp_id))
            {
                id_on_remove = tmp_id;

                break;
            }

            address += EEPROM_ALIAS_ROW_LENGTH;
        }

        if (id_on_remove != 0)
        {
            eeprom_alias_remove(&id_on_remove);
        }
        else
        {
            break;
        }
    }
}

void eeprom_alias_add(uint64_t *id, char *name)
{
    int i = _eeprom_alias_find_position_for_id(id);

    if (i == -1)
    {
        i = _eeprom.alias_length;
    }

    uint32_t address = EEPROM_ALIAS_ADDRESS_START + (i * EEPROM_ALIAS_ROW_LENGTH);

    uint64_t *pid = (uint64_t *)_eeprom.buffer;

    *pid = *id;

    memcpy(_eeprom.buffer + 8, name, 32);

    if (!bc_eeprom_write(address, _eeprom.buffer, EEPROM_ALIAS_ROW_LENGTH))
    {
        usb_talk_send_format("[\"$eeprom/alias/add/error\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", *id);

        return;
    }

    if (i == _eeprom.alias_length)
    {
        if (!_eeprom_alias_length_set(_eeprom.alias_length + 1))
        {
            usb_talk_send_format("[\"$eeprom/alias/add/error\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", *id);

            return;
        }
    }

    usb_talk_send_format("[\"$eeprom/alias/add/ok\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", *id);
}

void eeprom_alias_remove(uint64_t *id)
{
    int i = _eeprom_alias_find_position_for_id(id);

    if (i != -1)
    {
        if ((i != _eeprom.alias_length) && (_eeprom.alias_length > 1))
        {
            // laste record
            uint32_t address = EEPROM_ALIAS_ADDRESS_START + ((_eeprom.alias_length - 1) * EEPROM_ALIAS_ROW_LENGTH);

            bc_eeprom_read(address, _eeprom.buffer, EEPROM_ALIAS_ROW_LENGTH);

            address = EEPROM_ALIAS_ADDRESS_START + (i * EEPROM_ALIAS_ROW_LENGTH);

            if (!bc_eeprom_write(address, _eeprom.buffer, EEPROM_ALIAS_ROW_LENGTH))
            {
                usb_talk_send_format("[\"$eeprom/alias/remove/error\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", *id);

                return;
            }
        }

        if (!_eeprom_alias_length_set(_eeprom.alias_length - 1))
        {
            usb_talk_send_format("[\"$eeprom/alias/remove/error\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", *id);

            return;
        }
    }

    usb_talk_send_format("[\"$eeprom/alias/remove/ok\", \"" USB_TALK_DEVICE_ADDRESS "\"]\n", *id);
}

void eeprom_alias_list(int page)
{
    if (page >= EEPROM_ALIAS_PAGE_LENGTH)
    {
        return;
    }

    int max_i = (page + 1) * EEPROM_ALIAS_ON_PAGE;

    if (max_i >  _eeprom.alias_length)
    {
        max_i = _eeprom.alias_length;
    }

    uint32_t address;
    uint64_t *pid;
    bool comma = false;

    usb_talk_message_start("$eeprom/alias/list/%d", page);
    usb_talk_message_append("{");

    for (int i = page * EEPROM_ALIAS_ON_PAGE; i < max_i; i++)
    {
        address = EEPROM_ALIAS_ADDRESS_START + (i * EEPROM_ALIAS_ROW_LENGTH);

        bc_eeprom_read(address, _eeprom.buffer, EEPROM_ALIAS_ROW_LENGTH);

        if (comma)
        {
            usb_talk_message_append(",");
        }

        pid = (uint64_t *)_eeprom.buffer;

        usb_talk_message_append("\"" USB_TALK_DEVICE_ADDRESS "\": \"%s\"", *pid, _eeprom.buffer + 8);

        comma = true;
    }

    usb_talk_message_append("}");

    usb_talk_message_send();
}

void eeprom_alias_purge(void)
{
    _eeprom_alias_length_set(0);
}

static int _eeprom_alias_find_position_for_id(uint64_t *id)
{
    uint32_t address = EEPROM_ALIAS_ADDRESS_START;
    uint64_t tmp_id;

    for (int i = 0; i < _eeprom.alias_length; i++)
    {
        bc_eeprom_read(address, &tmp_id, sizeof(tmp_id));

        if (tmp_id == *id)
        {
            return i;
        }

        address += EEPROM_ALIAS_ROW_LENGTH;
    }

    return -1;
}

static bool _eeprom_alias_length_set(uint8_t length)
{
    uint8_t buffer[2];

    buffer[0] = length;
    buffer[1] = ~length;

    if (!bc_eeprom_write(EEPROM_ALIAS_ADDRESS_LENGTH, buffer, sizeof(buffer)))
    {
        return false;
    }

    _eeprom.alias_length = length;

    return true;
}
