#include "config.h"
#include "template_store.h"
#include "recognizer.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

/** Bytes of the record covered by the CRC: everything except the CRC itself. */
static const uint16_t CRC_SPAN = (uint16_t)(sizeof(StoredTemplate) - sizeof(uint16_t));

uint8_t eeprom_read_raw(uint16_t address)
{
    while (EECR & _BV(EEPE)) {
        /* a previous write is still in progress */
    }
    EEAR = address;
    EECR |= _BV(EERE);
    return EEDR;
}

void eeprom_write_raw(uint16_t address, uint8_t value)
{
    while (EECR & _BV(EEPE)) {
        /* wait out the previous 3.4 ms programming cycle */
    }
    EEAR = address;
    EEDR = value;

    /* EEPE must be set within four clock cycles of EEMPE. An interrupt landing
     * between the two stores would blow that window and silently drop the
     * write, so the pair is atomic. Timer0 loses at most one tick here. */
    uint8_t sreg = SREG;
    cli();
    EECR |= _BV(EEMPE);
    EECR |= _BV(EEPE);
    SREG = sreg;
}

bool template_load_from_eeprom(StoredTemplate *out)
{
    uint8_t *raw = (uint8_t *)out;
    for (uint16_t i = 0; i < sizeof(StoredTemplate); ++i) {
        raw[i] = eeprom_read_raw((uint16_t)(EEPROM_TEMPLATE_ADDR + i));
    }
    if (out->magic != TEMPLATE_MAGIC || out->version != TEMPLATE_VERSION) {
        return false;
    }
    return out->checksum == vpl_crc16(raw, CRC_SPAN);
}

void template_save_to_eeprom(StoredTemplate *record)
{
    record->magic = TEMPLATE_MAGIC;
    record->version = TEMPLATE_VERSION;
    record->reserved = 0;
    record->checksum = vpl_crc16((const uint8_t *)record, CRC_SPAN);

    const uint8_t *raw = (const uint8_t *)record;
    for (uint16_t i = 0; i < sizeof(StoredTemplate); ++i) {
        uint16_t addr = (uint16_t)(EEPROM_TEMPLATE_ADDR + i);
        if (eeprom_read_raw(addr) != raw[i]) {
            eeprom_write_raw(addr, raw[i]);
        }
    }
}

void template_clear(void)
{
    /* Erasing the two magic bytes is enough: without them the load rejects the
     * record whatever the rest of it says, and it costs two write cycles
     * instead of ninety. */
    eeprom_write_raw(EEPROM_TEMPLATE_ADDR + 0u, EEPROM_ERASED_BYTE);
    eeprom_write_raw(EEPROM_TEMPLATE_ADDR + 1u, EEPROM_ERASED_BYTE);
}
