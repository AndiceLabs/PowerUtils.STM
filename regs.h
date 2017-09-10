#ifndef __REGS_H__
#define __REGS_H__

//
// Firmware version is tied to registers and commands
//
#define FIRMWARE_MAJOR      0
#define FIRMWARE_MINOR      9


enum registers_type {
    REG_ID,                     // 00 - 0xED for all new AndiceLabs products
    REG_PROD,                   // 01 - Product ID/type
    REG_STEP,                   // 02 - Product stepping: P=0,A=1,B=2 - significant HW changes
    REG_REVISION,               // 03 - Product revision: small HW changes (BOM only)
    REG_VERSION_MAJOR,          // 04 - Firmware major version
    REG_VERSION_MINOR,          // 05 - Firmware minor version
    REG_RESET_FLAGS,            // 06 - MCU reset flags
    REG_STATUS,                 // 07 - Board status flags (button state, power good, armed WDTs, etc.)
    REG_CONTROL,                // 08 - Control flags (charge enable, AUX output, etc.)
    REG_START_ENABLE,           // 09 -
    REG_START_REASON,           // 0A -
    
    REG_DATA_0,                 // 0B - Command LSB data
    REG_DATA_1,                 // 0C - 
    REG_DATA_2,                 // 0D - 
    REG_DATA_3,                 // 0E - Command MSB data
    REG_COMMAND,                // 0F - Command register

    REG_WDT_POWER,              // 10 - Power-cycle watchdog countdown register (seconds, 0 to disable)
    REG_WDT_STOP,               // 11 - Power-off countdown (single-shot seconds, 0 to disable)
    REG_WDT_START,              // 12 - Start-up activity watchdog countdown (seconds, 0 to disable)
    
    REG_END,                    // 14 - 0xEE
    NUM_REGISTERS
};


// REG_ID is always 0xED  (I miss you, Dad!)

// REG_PROD types
#define PROD_UNKNOWN                0x00    // MFG unprogrammed
#define PROD_POWERCAPE              0x01    // VCC is tied to 3v3 
#define PROD_POWERHAT               0x02    // VCC is tied to GPIO26 for shutdown request 
#define PROD_POWERMODULE            0x03    // Aux power, no charger, Grove interface

// REG_STATUS bits
#define STATUS_POWER_GOOD           0x01    // PG state 
#define STATUS_BUTTON               0x02    // Button state
#define STATUS_OPTO                 0x04    // Opto state

// REG_CONTROL
#define CONTROL_AUX_OUTPUT          0x01
#define CONTROL_LED                 0x02
#define CONTROL_CHARGE_ENABLE       0x04
#define CONTROL_IGNORE_POWEROFF     0x08

// START enable and reason register bits
#define START_BUTTON                0x01    // External (power) button
#define START_EXTERNAL              0x02    // External (opto) signal
#define START_PWRGOOD               0x04    // DC power for battery charger products
#define START_TIMEOUT               0x08    // Countdown timeout
#define START_PWR_ON                0x10    // Initial application of power
#define START_WDT_RESET             0x20    // WDT cycled power
#define START_ALL                   0x1F


// ====================
// REG_COMMAND commands
// --------------------
// Self-test, set NVRAM defaults, lock device, set RTC, etc.
#define COMMAND_CHARGE_ENABLE       0x10
#define COMMAND_CHARGE_DISABLE      0x11
#define COMMAND_LED_ON_MS           0x12
#define COMMAND_LED_OFF_MS          0x13
#define COMMAND_EXT_PWR_ON          0x14
#define COMMAND_EXT_PWR_OFF         0x15
#define COMMAND_SET_RESTART_TIME    0x16
#define COMMAND_GET_RESTART_TIME    0x17
#define COMMAND_CLEAR_RESTART_TIME  0x18
#define COMMAND_GET_ONTIME          0x19
#define COMMAND_GET_OFFTIME         0x1A
#define COMMAND_GET_CHARGE_RATE     0x1B
#define COMMAND_SET_CHARGE_RATE_1   0x1C
#define COMMAND_SET_CHARGE_RATE_2   0x1D
#define COMMAND_SET_CHARGE_RATE_3   0x1E
#define COMMAND_FIRMWARE_TIMESTAMP  0x1F
#define COMMAND_READ_RTC            0x20
#define COMMAND_WRITE_RTC           0x21
#define COMMAND_SET_I2C_ADDRESS     0x22

#define COMMAND_GET_RTC_CAL         0x30
#define COMMAND_SET_RTC_CAL         0x31
#define COMMAND_EEPROM_CLEAR        0x3C
#define COMMAND_EEPROM_STORE        0x3E

#define COMMAND_REBOOT              0xBB


#endif  // __REGS_H__
