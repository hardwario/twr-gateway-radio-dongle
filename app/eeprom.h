#ifndef APP_EEPROM_H
#define APP_EEPROM_H

#include <bc_eeprom.h>

#define EEPROM_ALIAS_ADDRESS_LENGTH    0x00
#define EEPROM_ALIAS_ADDRESS_START     0x04
#define EEPROM_ALIAS_NAME_LENGTH       32
#define EEPROM_ALIAS_ROW_LENGTH        (8 + EEPROM_ALIAS_NAME_LENGTH)
#define EEPROM_ALIAS_ON_PAGE           8
#define EEPROM_ALIAS_PAGE_LENGTH       4


void eeprom_init(void);

void eeprom_alias_add(uint64_t *id, char *name);

void eeprom_alias_remove(uint64_t *id);

void eeprom_alias_list(int page);

void eeprom_alias_purge(void);


#endif // APP_EEPROM_H
