/* simavr's .mmcu descriptor for the benchmark firmware.
 *
 * Kept in its own C file because avr_mcu_section.h uses C99 designated
 * initialisers and a weak const, neither of which compiles as C++.
 *
 * It declares the MCU type and clock, and hands simavr two registers:
 * GPIOR0 as a console (bytes written there appear on stdout) and GPIOR1 as a
 * command port (writing 4 = SIMAVR_CMD_EXIT_CODE_0 ends the simulation).
 * Neither register is used for anything else in this project.
 */

#include <avr/io.h>
#include <avr/avr_mcu_section.h>

AVR_MCU(F_CPU, "atmega328p");
AVR_MCU_SIMAVR_CONSOLE(&GPIOR0);
AVR_MCU_SIMAVR_COMMAND(&GPIOR1);
