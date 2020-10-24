![PowerHAT](https://andicelabs.com/wp-content/uploads/2018/01/PowerHAT_PiAplus_battery-1024x577.jpg)
# Utilities for the Raspberry Pi Power HAT
These are the command line utilities for interfacing with the Power HAT's components.
## Building and Installing
First, clone this repository on your Raspberry Pi:
`git clone https://github.com/AndiceLabs/PowerUtils.STM`

Then, change into the directory and build:
```
cd PowerUtils.STM
make
```

You should wind up with two executables: **power** and **ina219**.  Copy these somewhere in your path.  I like to use ~/bin.

## INA219 Utility
The Power HAT has an INA219 current monitor on the Lithium Ion battery interface.  The **ina219** utility will read the voltage and current and display them in milli-volts and milli-amps.  Note that the current can be a negative number when the battery is being charged.  There are several options for changing the output:
```
Usage: ina219 <mode> 
   Mode (required):
      -h --help           Show usage.
      -i --interval       Set interval for monitor mode.
      -w --whole          Show whole numbers only. Useful for scripts.
      -v --voltage        Show battery voltage in mV.
      -c --current        Show battery current in mA.
      -a --address <addr> Override I2C address of INA219 from default of 0x40.
      -b --bus <i2c bus>  Override I2C bus from default of 1.
```
## Power Utility
The **power** utility interfaces with the HAT's power controller and has a number of options:
```
Usage: ./power [OPTION] 
   Options:
      -h --help               Show usage
      -a --address <addr>     Use HAT at I2C <addr> instead of 0x60
      -A --i2c <addr>         Set HAT I2C address to <addr>
      -b --bus <bus>          Use I2C <bus> instead of 2
      -B --battery <1-3>      Set battery charge rate in thirds of an amp
      -C                      Charger enable
      -c                      Charger disable (power will be lost if no battery!)
      -d --disable <setting>  Disable power-up setting:
                  button          Button pressed
                  opto            External opto signal
                  pgood           DC power good
                  timeout         Countdown timer
                  poweron         Initial power
                  auto-off        Auto power-off by VCC (cape) or GPIO26 (HAT)
      -e --enable  <setting>  Enable power-up setting (same as above)
      -k --killpower          Set power-off WDT timer (0-255 seconds)
      -p --power              External power off/on (0-1)
                              On the HAT/Cape, this is the external LED connector
      -q --query              Query board info
      -r --read               Read and display board RTC value
      -R --set                Set system time from RTC
      -s --store              Store current settings in EEPROM
      -t --timeout            Set power-on timeout value
      -v --value <setting>    Return numeric value (for scripts) of:
                  button          Button pressed (0-1)
                  pgood           DC power good (0-1)
                  rate            Charge rate (1-3)
                  ontime          Powered duration (seconds)
                  offtime         Last power off duration (seconds)
                  restart         Power-up restart timer (seconds)
      -w --write              Write RTC from system time
      -X --calibrate          Set RTC calibration value
      -x                      Read RTC calibration value
      -z --reset              Restart power controller
      -Z --upload <file>      Upload firmware image
```
