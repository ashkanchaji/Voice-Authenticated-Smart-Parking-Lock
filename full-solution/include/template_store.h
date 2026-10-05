#ifndef TEMPLATE_STORE_H
#define TEMPLATE_STORE_H

/* Persistent template storage in the on-chip EEPROM, at register level.
 *
 * The EEPROM keeps its contents across reset and power loss, which is the
 * whole point: a parking gate that forgot its authorised speaker every time
 * the power blinked would be useless. 1 KB is available and the record below
 * uses 90 bytes of it, all at EEPROM_TEMPLATE_ADDR.
 *
 * avr-libc's <avr/eeprom.h> would also work. The register-level version is
 * here because the timed EEMPE/EEPE write sequence is one of the few places
 * on this chip where the datasheet's four-cycle window actually matters, and
 * that is worth seeing once.
 */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/** On-EEPROM layout. Read and written as a flat byte image. */
struct StoredTemplate {
    uint16_t magic;                  /**< TEMPLATE_MAGIC when written by us */
    uint8_t version;                 /**< TEMPLATE_VERSION; layout generation */
    uint8_t reserved;                /**< pad to an even offset, must be 0 */
    uint8_t features[FEATURE_COUNT]; /**< averaged template, 0..255 per feature */
    uint32_t threshold;              /**< accept if distance <= this */
    uint16_t checksum;               /**< CRC-16/CCITT over everything above */
};

/** Read the record and verify magic, version and CRC.
 *
 * Returns false for a blank EEPROM (0xFF everywhere), a record written by an
 * older feature layout, or a corrupted one. The caller's response to all three
 * is the same: announce TRAINING REQUIRED and collect new utterances.
 */
bool template_load_from_eeprom(StoredTemplate *out);

/** Write the record, filling in magic, version and checksum.
 *
 * Only bytes whose value actually changes are written. An EEPROM cell is rated
 * for 100k writes and each write takes 3.4 ms, so skipping the unchanged ones
 * saves both endurance and about a quarter second of the training sequence.
 */
void template_save_to_eeprom(StoredTemplate *record);

/** Invalidate the stored template by erasing its magic word. */
void template_clear(void);

/** Register-level EEPROM byte read. */
uint8_t eeprom_read_raw(uint16_t address);

/** Register-level EEPROM byte write, blocking until the cell is programmed. */
void eeprom_write_raw(uint16_t address, uint8_t value);

#endif /* TEMPLATE_STORE_H */
