#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <endian.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include "regs.h"

#define BLOCK_I2C_WRITE     16
#define I2C_DELAY_MS        50

#define STM_ADDRESS         0x60
#define INA_ADDRESS         0x40

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;

typedef enum
{
    OP_NONE,
    OP_CHARGE,
    OP_DISABLE,
    OP_ENABLE,
    OP_CHARGE_DISABLE,
    OP_CHARGE_ENABLE,
    OP_EEPROM,
    OP_SET_TIMEOUT,
    OP_READ_RTC,
    OP_SET_SYSTIME,
    OP_WRITE_RTC,
    OP_READ_CAL,
    OP_WRITE_CAL,
    OP_QUERY,
    OP_RESET,
    OP_UPLOAD,
    OP_SET_I2C,
    OP_VALUE,
    OP_EXT_POWER,
} op_type;

op_type operation = OP_NONE;
char *oper_arg = NULL;

#ifdef BEAGLEBONE
 int i2c_bus = 2;
#else
 int i2c_bus = 1;
#endif
int stm_address = STM_ADDRESS;
int new_address = 0;
int charge_rate = 1;
int power_timeout = 0;
int calibration_value = 0;
int handle = 0;

#define MAX_IMAGE_SIZE      ( 1024 * 16 )
#define FLASH_PAGE_SIZE     ( 128 )
#define HALF_PAGE_SIZE      ( FLASH_PAGE_SIZE / 2 )
char *filename;
int  filehandle;


void msleep ( int msecs )
{
    usleep ( msecs * 1000 );
}


int i2c_read ( void *buf, int len )
{
    int rc = 0;

    if ( read ( handle, buf, len ) != len )
    {
        fprintf ( stderr, "I2C read failed: %s\n", strerror ( errno ) );
        rc = -1;
    }

    return rc;
}


int i2c_write( void *buf, int len )
{
    int rc = 0;

    if ( write( handle, buf, len ) != len )
    {
        fprintf( stderr, "I2C write failed: %s\n", strerror ( errno ) );
        rc = -1;
    }

    return rc;
}


int register_read( unsigned char reg, unsigned char *data )
{
    int rc = -1;
    unsigned char bite[ 4 ];

    bite[ 0 ] = reg;
    if ( i2c_write( bite, 1 ) == 0 )
    {
        if ( i2c_read( bite, 1 ) == 0 )
        {
            *data = bite[ 0 ];
            rc = 0;
        }
    }

    return rc;
}


// TODO: Figure out why I can't block read on the Pi
#if 0
int register32_read( unsigned char reg, unsigned int *data )
{
    int rc = -1;
    unsigned char bite[ 4 ];

    bite[ 0 ] = reg;
    if ( i2c_write( bite, 1 ) == 0 )
    {
        if ( i2c_read( data, 4 ) == 0 )
        {
            rc = 0;
        }
    }

    return rc;
}
#endif


int data32_read( uint32_t *data )
{
    int i;
    uint8_t bite;

    *data = 0;
    
    for ( i = 0; i < 4; i++ )
    {
        bite = REG_DATA_3 - i;
        if ( i2c_write( &bite, 1 ) == 0 )
        {
            if ( i2c_read( &bite, 1 ) == 0 )
            {
                *data <<= 8;
                *data |= bite;
            }
            else return -1;
        }
        else return -1;
    }

    return 0;
}


int data32_write( uint32_t data )
{
    int i;
    uint8_t bite[ 2 ];

    for ( i = 0; i < 4; i++ )
    {
        bite[ 0 ] = REG_DATA_0 + i;
        bite[ 1 ] = data & 0xFF;
        data >>= 8;
        if ( i2c_write( &bite, 2 ) != 0 )
        {
            return -1;
        }
    }

    return 0;
}


int register_write( unsigned char reg, unsigned char data )
{
    int rc = -1;
    unsigned char bite[ 4 ];

    bite[ 0 ] = reg;
    bite[ 1 ] = data;

    if ( i2c_write( bite, 2 ) == 0 )
    {
        rc = 0;
    }

    return rc;
}


int register_block_write( unsigned char reg, unsigned char *data, unsigned char len )
{
    int i;
    int rc = -1;
    unsigned char bites[ BLOCK_I2C_WRITE+1 ];

    if ( len > BLOCK_I2C_WRITE )
        return rc;
    
    bites[ 0 ] = reg;
    for ( i=0; i<len; i++ )
    {
        bites[ i+1 ] = *data++;
    }

    if ( i2c_write( bites, len+1 ) == 0 )
    {
        rc = 0;
    }

    return rc;
}


int command_wait( uint8_t command )
{
    uint8_t r = 0xEE;
    
    if ( register_write( REG_COMMAND, command ) == 0 )
    {
        do {
            msleep( 1 );
            if ( register_read( REG_COMMAND, &r ) != 0 )
                break;
        } while ( r == command );
        
        switch ( r )
        {
            case 0xEA:
            {
                fprintf( stderr, "Command error: invalid address\n" );
                break;
            }
            case 0xEC:
            {
                fprintf( stderr, "Command error: invalid command\n" );
                break;
            }
            case 0xEE:
            {
                fprintf( stderr, "Command or state error\n" );
                break;
            }
            default:
            case 0:
            {
                break;
            }
        }
    }
    
    return (int)r;
}


int command_read8( uint8_t command, uint8_t *data )
{
    int rc = -1;
    
    if ( command_wait( command ) == 0 )
    {
        if ( register_read( REG_DATA_0, data ) == 0 )
        {
            rc = 0;
        }
    }
    
    return rc;
}


int command_read32( uint8_t command, uint32_t *data )
{
    int rc = -1;
    
    if ( command_wait( command ) == 0 )
    {
        if ( data32_read( data ) == 0 )
        {
            rc = 0;
        }
    }
    
    return rc;
}


int command_write32( uint8_t command, uint32_t data )
{
    int rc = -1;
    
    if ( data32_write( data ) == 0 )
    {
        if ( command_wait( command ) == 0 )
        {
            rc = 0;
        }
    }

    return rc;
}


int verify_product( void )
{
    int rc = 0;
    uint8_t c;
    
    if ( register_read( REG_ID, &c ) == 0 )
    {
        if ( c == 0xED ) 
        {
            rc = 1;
        }
    }
    
    return rc;
}


void print_duration( uint32_t seconds )
{
    int d, h, m;
    
    d = h = m = 0;
    
    if ( seconds > 86400 )
    {
        d = seconds / 86400;
        seconds -= ( d * 86400 );
        printf( "%d days and ", d );
    }
    if ( seconds > 3600 )
    {
        h = seconds / 3600;
        seconds -= ( 3600 * h );
    }
    if ( seconds > 60 )
    {
        m = seconds / 60;
        seconds -= ( 60 * m );
    }
    printf( "%02d:%02d:%02d", h, m, seconds );
}


int cape_read_rtc( time_t *iptr )
{
    int rc = 1;
    uint32_t seconds;

    if ( command_read32( COMMAND_READ_COUNT, &seconds ) == 0 )
    {
        if ( iptr != NULL )
        {
            *iptr = seconds;
        }
        rc = 0;
    }

    return rc;
}


int cape_show_rtc( void )
{
    int rc;
    time_t seconds;
    
    rc = cape_read_rtc( &seconds );
    if ( rc == 0 )
    {
        printf( "Cape RTC seconds %08X (%d)\n", seconds, seconds );
        printf( ctime( (time_t*)&seconds ) );
    }
    else fprintf( stderr, "Error reading board RTC\n" );
    
    return rc;
}


int cape_write_rtc( void )
{
    struct timeval t;
    int rc = 1;

    // Align close to system second
    gettimeofday( &t, NULL );
    usleep( 999999 - t.tv_usec );
    gettimeofday( &t, NULL );
    
    if ( command_write32( COMMAND_WRITE_COUNT, t.tv_sec ) == 0 )
    {
        rc = 0;
        printf( "System seconds %08X (%d)\n", t.tv_sec, t.tv_sec );
        printf( ctime( &t.tv_sec ) );
    }
    else fprintf( stderr, "Error writing board RTC\n" );

    return rc;
}


int cape_show_cape_info( void )
{
    uint8_t c;
    uint8_t product, step, revision;
    uint8_t ver_maj, ver_min;
    uint32_t time, d;
    
    if ( register_read( REG_PROD, &product ) == 0 )
    {
        printf( "\nProduct      : " );
        if ( product == PROD_POWERCAPE ) printf( "PowerCape" );
        else if ( product == PROD_POWERHAT ) printf( "PowerHAT" );
        else if ( product == PROD_POWERMODULE ) printf( "Power Module" );
        else printf( "Unknown" );
        printf( "\n" );
    }
    else return -1;
    
    if ( register_read( REG_STEP, &step ) != 0 )
        return -1;
    
    if ( register_read( REG_REVISION, &revision ) == 0 )
    {
        if ( isprint( step ) && isprint( revision ) )
        {
            printf( "HW Revision  : %c%c\n", step, revision );
        }
    }
    else return -1;
    
    if ( register_read( REG_VERSION_MAJOR, &ver_maj ) != 0 )
        return -1;
    if ( register_read( REG_VERSION_MINOR, &ver_min ) != 0 )
        return -1;
    printf( "Interface    : v%d.%d\n", ver_maj, ver_min );

    if ( command_read32( COMMAND_GET_SERIAL, &d ) == 0 )
    {
        int i;
        char ser[4];
        
        for( i=0; i<4; i++ )
        {
            ser[i] = d & 0xFF;
            d >>= 8;
        }
        printf( "HW Serial#   : %c%c%c%c\n", ser[3], ser[2], ser[1], ser[0] );
    }
    
    if ( command_read32( COMMAND_GET_TIMESTAMP, &time ) == 0 )
    {
        printf( "HW Build     : %s\n", ctime( (time_t*)&time ) );
    }
    
    if ( command_read32( COMMAND_FIRMWARE_TIMESTAMP, &time ) == 0 )
    {
        printf( "Firmware     : %s", ctime( (time_t*)&time ) );
    }
    else return -1;    
    
    if ( command_read8( COMMAND_GET_CHARGE_RATE, &c ) == 0 )
    {
        printf( "Charge rate  : %d/3 amp\n", c );
    }
    else return -1;    
    
    if ( command_read32( COMMAND_GET_RESTART_TIME, &time ) == 0 )
    {
        printf( "Restart time : %d seconds (", time );
        print_duration( time );
        printf( ")\n" );
    }
    else return -1;    

    if ( command_read32( COMMAND_GET_ONTIME, &time ) == 0 )
    {
        printf( "On time      : " );
        print_duration( time );
        printf( "\n" );
    }
    else return -1;    
    
    if ( command_read32( COMMAND_GET_OFFTIME, &time ) == 0 )
    {
        printf( "Off time     : " );
        print_duration( time );
        printf( "\n" );
    }
    else return -1;    

    if ( register_read( REG_STATUS, &c ) == 0 )
    {
        printf( "Status       : " );
        if ( c == 0 ) 
        {
            printf( "none " );
        }
        else
        {
            if ( c & STATUS_POWER_GOOD ) printf( "PGOOD " );
            if ( c & STATUS_BUTTON ) printf( "BUTTON " );
            if ( c & STATUS_OPTO ) printf( "OPTO " );
            if ( c & STATUS_LED ) printf( "LED " );
            if ( c & STATUS_EXT_POWER ) printf( "EXT_PWR" );
        }
        printf ( "\n\n" );
    }
    else return -1;
    
    if ( register_read( REG_START_REASON, &c ) == 0 )
    {
        printf( "Power on triggered by " );
        if ( c == 0 ) 
        {
            printf( "nothing " );
        }
        else
        {
            if ( c & START_BUTTON ) printf( "button press " );
            if ( c & START_EXTERNAL ) printf( "external event " );
            if ( c & START_PWRGOOD ) printf( "power good " );
            if ( c & START_TIMEOUT ) printf( "timer " );
            if ( c & START_PWR_ON ) printf( "inital power " );
            if ( c & START_WDT_RESET ) printf( "watchdog reset " );
        }
        printf ( "\n\n" );
    }
    else return -1;

    return 0;
}


int get_enable_mask( uint8_t *mask )
{
    if ( strcasecmp( oper_arg, "button" ) == 0 )
    {
        *mask = START_BUTTON;        
    }
    else if ( strcasecmp( oper_arg, "opto" ) == 0 )
    {
        *mask = START_EXTERNAL;        
    }
    else if ( strcasecmp( oper_arg, "pgood" ) == 0 )
    {
        *mask = START_PWRGOOD;        
    }
    else if ( strcasecmp( oper_arg, "timeout" ) == 0 )
    {
        *mask = START_TIMEOUT;        
    }
    else if ( strcasecmp( oper_arg, "poweron" ) == 0 )
    {
        *mask = START_PWR_ON;
    }
    else
    {
        return -1;
    }
    return 0;
}


int cape_enable_setting( void )
{
    uint8_t mask, b;

    if ( get_enable_mask( &mask ) != 0 )
    {
        fprintf( stderr, "Unknown enable setting: %s.\n", oper_arg );
        return -1;
    }        
    
    if ( register_read( REG_START_ENABLE, &b ) == 0 )
    {
        if ( ( b & mask ) == 0 )
        {
            register_write( REG_START_ENABLE, b | mask );
            printf( "Start on %s enabled\n", oper_arg );
        }
        else
        {
            printf( "Start on %s already enabled\n", oper_arg );
        }
    }
    else
    {
        fprintf( stderr, "Error accessing start register\n" );
        return -1;
    }

    return 0;
}


int cape_disable_setting( void )
{
    uint8_t mask, b;

    if ( get_enable_mask( &mask ) != 0 )
    {
        fprintf( stderr, "Unknown disable setting: %s.\n", oper_arg );
        return -1;
    }        
    
    if ( register_read( REG_START_ENABLE, &b ) == 0 )
    {
        if ( b & mask )
        {
            register_write( REG_START_ENABLE, b & ~mask );
            printf( "Start on %s disabled\n", oper_arg );
        }
        else
        {
            printf( "Start on %s already disabled\n", oper_arg );
        }
    }
    else
    {
        fprintf( stderr, "Error accessing start register\n" );
        return -1;
    }

    return 0;
}


int cape_set_charge_rate( void )
{
    uint8_t cmd = ( charge_rate-1 ) + COMMAND_SET_CHARGE_RATE_1;
    
    return command_wait( cmd );
}


int cape_write_timeout( void )
{
    int rc = 1;

    printf( "Setting restart timer to %d\n", power_timeout );

    if ( command_write32( COMMAND_SET_RESTART_TIME, power_timeout ) == 0 )
    {
        rc = 0;
    }
    else fprintf( stderr, "Error writing restart timer\n" );

    return rc;
}


int cape_read_calibration( void )
{
    int rc = 1;
    uint32_t value;
    int i;

    if ( command_read32( COMMAND_GET_RTC_CAL, &value ) == 0 )
    {
        if ( value & 0x8000 )
        {
            i = 512 - ( value & 0x1FF );
        }
        else
        {
            i = 0 - ( value & 0x1FF );
        }
        printf( "Cape RTC calibration %04X (%d)\n", value, i );
        rc = 0;
    }
    else fprintf( stderr, "Error reading RTC calibration value\n" );

    return rc;
    
}


int cape_write_calibration( void )
{
    int rc = 1;
    uint32_t v;

    if ( calibration_value > 0 )
    {
        v = ( 512 - calibration_value ) | 0x8000;
    }
    else
    {
        v = abs( calibration_value ) & 0x1FF;
    }
    printf( "Setting calibration value %d (%04X)\n", calibration_value, v );
    
    if ( command_write32( COMMAND_SET_RTC_CAL, v ) == 0 )
    {
        rc = 0;
    }
    else fprintf( stderr, "Error writing RTC calibration value\n" );

    return rc;
}


int cape_write_eeprom( void )
{
    return command_wait( COMMAND_EEPROM_STORE );
}


int cape_reset( void )
{
    return command_wait( COMMAND_REBOOT );
}


int cape_enable_charger( void )
{
    return command_wait( COMMAND_CHARGE_ENABLE );
}


int cape_disable_charger( void )
{
    return command_wait( COMMAND_CHARGE_DISABLE );
}


int  cape_set_address( int new_addr )
{
    return command_write32( COMMAND_SET_I2C_ADDRESS, new_addr );
}


int cape_show_value( void )
{
    uint32_t d;
    uint8_t b;
    
    if ( strcasecmp( oper_arg, "button" ) == 0 )
    {
        if ( register_read( REG_STATUS, &b ) )
            return 2;
        
        if ( b & STATUS_BUTTON )
            printf( "1\n" );
        else
            printf( "0\n" );
    }
    else if ( strcasecmp( oper_arg, "pgood" ) == 0 )
    {
        if ( register_read( REG_STATUS, &b ) )
            return 2;
        
        if ( b & STATUS_POWER_GOOD )
            printf( "1\n" );
        else
            printf( "0\n" );
    }
    else if ( strcasecmp( oper_arg, "rate" ) == 0 )
    {
        if ( command_read8( COMMAND_GET_CHARGE_RATE, &b ) == 0 )
        {
            printf( "%d\n", b );
        }
        else return 2;
    }
    else if ( strcasecmp( oper_arg, "ontime" ) == 0 )
    {
        if ( command_read32( COMMAND_GET_ONTIME, &d ) == 0 )
        {
            printf( "%d\n", d );
        }
        else return 2;
    }
    else if ( strcasecmp( oper_arg, "offtime" ) == 0 )
    {
        if ( command_read32( COMMAND_GET_OFFTIME, &d ) == 0 )
        {
            printf( "%d\n", d );
        }
        else return 2;
    }
    else if ( strcasecmp( oper_arg, "restart" ) == 0 )
    {
        if ( command_read32( COMMAND_GET_RESTART_TIME, &d ) == 0 )
        {
            printf( "%d\n", d );
        }
        else return 2;
    }
    
    return 0;
}


int set_external_power( void )
{
    if ( ( strcasecmp( oper_arg, "1" ) == 0 ) ||
         ( strcasecmp( oper_arg, "on" ) == 0 ) ||
         ( strcasecmp( oper_arg, "yes" ) == 0 ) ||
         ( strcasecmp( oper_arg, "true" ) == 0 ) )
    {
        return command_wait( COMMAND_EXT_PWR_ON );
    }
    else if ( ( strcasecmp( oper_arg, "0" ) == 0 ) ||
         ( strcasecmp( oper_arg, "off" ) == 0 ) ||
         ( strcasecmp( oper_arg, "no" ) == 0 ) ||
         ( strcasecmp( oper_arg, "false" ) == 0 ) )
    {
        return command_wait( COMMAND_EXT_PWR_OFF );
    }
    else fprintf( stderr, "Unknown argument for external power command." );
}


void boot_erase_flash( uint8_t addr )
{
    register_write( BOOT_REG_ADDR, addr );
    msleep(1);
    register_write( BOOT_REG_CMD, BOOT_CMD_PAGE_ERASE );
}


void boot_program_flash( uint8_t addr, uint8_t *data )
{
    int i;
    
    register_write( BOOT_REG_ADDR, addr );
    msleep(1);
    for ( i = 0; i < HALF_PAGE_SIZE; i += BLOCK_I2C_WRITE )
    {
        register_block_write( BOOT_REG_DATA, data, BLOCK_I2C_WRITE );
        data += BLOCK_I2C_WRITE;
        msleep(1);
    }
    register_write( BOOT_REG_CMD, BOOT_CMD_HALF_PAGE_PROG );
}


int boot_enter( void )
{
    uint8_t b = 0;
    int rc = 0;
    
    register_read( REG_ID, &b );
    if ( b != 0xBB )
    {
        register_read( REG_STATUS, &b );
        if ( b & STATUS_BOOTLOADER )
        {
            printf( "Entering bootloader...\n" );
            
            if ( command_wait( COMMAND_ENTER_BOOTLOADER ) == 0 )
            {
                sleep( 2 );
                
                register_read( REG_ID, &b );
                if ( b == 0xBB )
                {
                    printf( "Done.\n" );
                }
                else
                {
                    printf( "Failed.\n" );
                    rc = 1;
                }
            }
            else
            {
                fprintf( stderr, "Bootloader entry failed.\n" );
                rc = 1;
            }
        }
        else
        {
            fprintf( stderr, "Bootloader not present!\n" );
            rc = 1;
        }
    }
    else
    {
        register_read( REG_PROD, &b );
        printf( "Found bootloader level %d.\n", b );
    }
    
    return rc;
}


void boot_execute( void )
{
    register_write( BOOT_REG_CMD, BOOT_CMD_EXECUTE );
}


int boot_upload( void )
{
    int fsize, count;
    void *memblock;
    uint8_t *ptr;
    uint8_t halfpage = 0;
    int rc = 0;
    
    if ( boot_enter() != 0 )
    {
        return 1;
    }
    
    filehandle = open( filename, O_RDONLY );
    
    if ( filehandle < 0 )
    {
        fprintf( stderr, "Error opening file %s\n", filename );
        return 3;
    }
    
    memblock = calloc( MAX_IMAGE_SIZE, 1 );
    if ( !memblock )
    {
        fprintf( stderr, "Error allocating memory\n" );
        return 3;
    }
    
    fsize = read( filehandle, memblock, MAX_IMAGE_SIZE );
    close( filehandle );
    printf( "%d bytes read\n", fsize );
    
    if ( *(uint32_t*)memblock != 0x200007FF )
    {
        ptr = memblock;
        while ( fsize > 0 )
        {
            boot_erase_flash( halfpage );
            msleep( I2C_DELAY_MS );
            
            boot_program_flash( halfpage, ptr );
            msleep( I2C_DELAY_MS );
            
            halfpage++;
            ptr += HALF_PAGE_SIZE;
            fsize -= HALF_PAGE_SIZE;
            if ( fsize < 0 ) break;
            
            boot_program_flash( halfpage, ptr );
            msleep( I2C_DELAY_MS );
            
            halfpage++;
            ptr += HALF_PAGE_SIZE;
            fsize -= HALF_PAGE_SIZE;
        }
        
        boot_execute();
    }
    else
    {
        fprintf( stderr, "Error: image not encoded\n" );
        rc = 2;
    }
    
    free( memblock );
    return rc;
}


void show_usage( char *progname )
{
    fprintf( stderr, "Usage: %s [OPTION] \n", progname );
    fprintf( stderr, "   Options:\n" );
    fprintf( stderr, "      -h --help               Show usage\n" );
    fprintf( stderr, "      -a --address <addr>     Use HAT at I2C <addr> instead of 0x%02X\n", STM_ADDRESS );
    fprintf( stderr, "      -A --i2c <addr>         Set HAT I2C address to <addr>\n" );
    fprintf( stderr, "      -b --bus <bus>          Use I2C <bus> instead of %d\n", i2c_bus );
    fprintf( stderr, "      -B --battery <1-3>      Set battery charge rate in thirds of an amp\n" );
    fprintf( stderr, "      -C                      Charger enable\n" );
    fprintf( stderr, "      -c                      Charger disable (power will be lost if no battery!)\n" );
    fprintf( stderr, "      -d --disable <setting>  Disable power-up setting:\n" );
    fprintf( stderr, "                  button          Button pressed\n" );
    fprintf( stderr, "                  opto            External opto signal\n" );
    fprintf( stderr, "                  pgood           DC power good\n" );
    fprintf( stderr, "                  timeout         Countdown timer\n" );
    fprintf( stderr, "                  poweron         Initial power\n" );
    fprintf( stderr, "                  auto-off        Auto power-off by VCC (cape) or GPIO26 (HAT)\n" );
    fprintf( stderr, "      -e --enable  <setting>  Enable power-up setting (same as above)\n" );
    fprintf( stderr, "      -p --power              External power off/on (0-1)\n" );
    fprintf( stderr, "                              On the HAT/Cape, this is the external LED connector\n" );
    fprintf( stderr, "      -q --query              Query board info\n" );
    fprintf( stderr, "      -r --read               Read and display board RTC value\n" );
    fprintf( stderr, "      -R --set                Set system time from RTC\n" );
    fprintf( stderr, "      -s --store              Store current settings in EEPROM\n" );
    fprintf( stderr, "      -t --timeout            Set power-on timeout value\n" );
    fprintf( stderr, "      -v --value <setting>    Return numeric value (for scripts) of:\n" );
    fprintf( stderr, "                  button          Button pressed (0-1)\n" );
    fprintf( stderr, "                  pgood           DC power good (0-1)\n" );
    fprintf( stderr, "                  rate            Charge rate (1-3)\n" );
    fprintf( stderr, "                  ontime          Powered duration (seconds)\n" );
    fprintf( stderr, "                  offtime         Last power off duration (seconds)\n" );
    fprintf( stderr, "                  restart         Power-up restart timer (seconds)\n" );
    fprintf( stderr, "      -w --write              Write RTC from system time\n" );
    fprintf( stderr, "      -X --calibrate          Set RTC calibration value\n" );
    fprintf( stderr, "      -x                      Read RTC calibration value\n" );
    fprintf( stderr, "      -z --reset              Restart power controller\n" );
    fprintf( stderr, "      -Z --upload <file>      Upload firmware image\n" );
    fprintf( stderr, "\n" );
    exit( 1 );
}


void parse( int argc, char *argv[] )
{
    while ( 1 )
    {
        static const struct option lopts[] =
        {
            { "help",       0,  NULL,   'h'   },
            { "addr",       1,  NULL,   'a'   },
            { "i2c",        1,  NULL,   'A'   },
            { "bus",        1,  NULL,   'b'   },
            { "battery",    1,  NULL,   'B'   },
            { "disable",    1,  NULL,   'd'   },
            { "enable",     1,  NULL,   'e'   },
            { "power",      1,  NULL,   'p'   },
            { "query",      0,  NULL,   'q'   },
            { "store",      0,  NULL,   's'   },
            { "timeout",    1,  NULL,   't'   },
            { "read",       0,  NULL,   'r'   },
            { "set",        0,  NULL,   's'   },
            { "value",      1,  NULL,   'v'   },
            { "write",      0,  NULL,   'w'   },
            { "calibrate",  1,  NULL,   'X'   },
            { "reset",      0,  NULL,   'z'   },
            { "upload",     1,  NULL,   'Z'   },
            { NULL,         0,  NULL,    0    },
        };
        int c;

        c = getopt_long( argc, argv, "?a:A:b:B:cCd:e:h:mn:p:qrRst:v:wxX:zZ:", lopts, NULL );

        if ( c == -1 )
            break;

        switch ( c )
        {
            case 'a':
            {
                int i;
                
                i = (int)strtol( optarg, NULL, 0 );
                if ( ( i >= 0x08 ) && ( i <= 0x77 ) )
                {
                    stm_address = i;
                }
                else
                {
                    fprintf( stderr, "Invalid I2C address\n" );
                }
                break;
            }
            
            case 'A':
            {
                int i;
                
                i = (int)strtol( optarg, NULL, 0 );
                if ( ( i >= 0x08 ) && ( i <= 0x77 ) )
                {
                    new_address = i;
                    operation = OP_SET_I2C;
                }
                else
                {
                    fprintf( stderr, "Invalid I2C address\n" );
                }
                break;
            }

            case 'b':
            {
                int i;
                
                errno = 0;
                i = (int)strtol( optarg, NULL, 0 );
                if ( errno == 0 )
                {
                    i2c_bus = i;
                }
                break;
            }

            case 'B':
            {
                int i;
                
                i = atoi( optarg );
                if ( ( i >= 1 ) && ( i <= 3 ) )
                {
                    charge_rate = i;
                    operation = OP_CHARGE;
                }
                else
                {
                    fprintf( stderr, "Invalid charge rate\n" );
                    operation = OP_NONE;
                }
                break;
            }
            
            case 'c':
            {
                operation = OP_CHARGE_DISABLE;
                break;
            }
            
            case 'C':
            {
                operation = OP_CHARGE_ENABLE;
                break;
            }
            
            case 'd':
            {
                if ( optarg != NULL )
                {
                    oper_arg = optarg;
                    operation = OP_DISABLE;
                }
                else
                {
                    fprintf( stderr, "Missing setting for disable\n" );
                    operation = OP_NONE;
                }
                break;
            }
            
            case 'e':
            {
                if ( optarg != NULL )
                {
                    oper_arg = optarg;
                    operation = OP_ENABLE;
                }
                else
                {
                    fprintf( stderr, "Missing setting for enable\n" );
                    operation = OP_NONE;
                }
                break;
            }

            case 'p':
            {
                if ( optarg != NULL )
                {
                    oper_arg = optarg;
                    operation = OP_EXT_POWER;
                }
                else
                {
                    fprintf( stderr, "Missing setting for enable\n" );
                    operation = OP_NONE;
                }
                break;
            }

            case 'q':
            {
                operation = OP_QUERY;
                break;
            }

            case 's':
            {
                operation = OP_EEPROM;
                break;
            }
            
            case 't':
            {
                power_timeout = atoi( optarg );
                operation = OP_SET_TIMEOUT;
                break;
            }

            case 'r':
            {
                operation = OP_READ_RTC;
                break;
            }

            case 'R':
            {
                operation = OP_SET_SYSTIME;
                break;
            }

            case 'v':
            {
                if ( optarg != NULL )
                {
                    oper_arg = optarg;
                    operation = OP_VALUE;
                }
                else
                {
                    fprintf( stderr, "Missing setting for disable\n" );
                    operation = OP_NONE;
                }
                break;
            }

            case 'w':
            {
                operation = OP_WRITE_RTC;
                break;
            }

            case 'x':
            {
                operation = OP_READ_CAL;
                break;
            }

            case 'X':
            {
                // Range is 512 to -511 CLK pulses
                calibration_value = atoi( optarg );
                if ( ( calibration_value <= 512 ) && ( calibration_value >= -511 ) )
                {
                    operation = OP_WRITE_CAL;
                }
                else
                {
                    fprintf( stderr, "Invalid calibration value\n" );
                    operation = OP_NONE;
                }
                break;
            }

            case 'z':
            {
                operation = OP_RESET;
                break;
            }

            case 'Z':
            {
                operation = OP_UPLOAD;
                filename = optarg;
                break;
            }

            case '?':
            {
                operation = OP_NONE;
                show_usage ( argv[ 0 ] );
                break;
            }
        }
    }
}


int main( int argc, char *argv[] )
{
    int rc = 0;
    char filename[ 20 ];

    if ( argc == 1 )
    {
        show_usage( argv[ 0 ] );
    }

    parse( argc, argv );

    if ( operation != OP_VALUE )
        printf( "Using I2C bus %d\n", i2c_bus );
    
    snprintf( filename, 19, "/dev/i2c-%d", i2c_bus );
    handle = open( filename, O_RDWR );
    if ( handle < 0 )
    {
        fprintf( stderr, "Error opening device %s: %s\n", filename, strerror ( errno ) );
        exit( 1 );
    }

    if ( ioctl( handle, I2C_SLAVE, stm_address ) < 0 )
    {
        close( handle );
        fprintf( stderr, "IOCTL Error: %d\n", strerror ( errno ) );
        exit( 1 );
    }

    if ( operation != OP_UPLOAD ) 
    {
        if ( verify_product() )
        {
            if ( operation != OP_VALUE )
                printf( "Board found at address 0x%X\n", stm_address );
        }
        else
        {
            close( handle );
            fprintf( stderr, "No board found at 0x%X\n", stm_address );
            exit( 1 );
        }
    }
    
    switch ( operation )
    {
        case OP_QUERY:
        {
            rc = cape_show_cape_info();
            break;
        }

        case OP_CHARGE:
        {
            rc = cape_set_charge_rate();
            break;
        }

        case OP_CHARGE_ENABLE:
        {
            rc = cape_enable_charger();
            break;
        }
        
        case OP_CHARGE_DISABLE:
        {
            rc = cape_disable_charger();
            break;
        }
        
        case OP_ENABLE:
        {
            rc = cape_enable_setting();
            break;
        }
        
        case OP_DISABLE:
        {
            rc = cape_disable_setting();
            break;
        }

        case OP_EEPROM:
        {
            rc = cape_write_eeprom();
            break;
        }
        
        case OP_SET_TIMEOUT:
        {
            rc = cape_write_timeout();
            break;
        }
        
        case OP_READ_RTC:
        {
            rc = cape_show_rtc();
            break;
        }

        case OP_SET_SYSTIME:
        {
            struct timeval t;

            rc = cape_read_rtc( &t.tv_sec );
            if ( rc == 0 )
            {
                t.tv_usec = 0;
                rc = settimeofday( &t, NULL );
                if ( rc != 0 )
                {
                    fprintf( stderr, "Error: %s\n", strerror( errno ) );
                }
            }
            break;
        }

        case OP_WRITE_RTC:
        {
            rc = cape_write_rtc();
            break;
        }
        
        case OP_READ_CAL:
        {
            rc = cape_read_calibration();
            break;
        }

        case OP_WRITE_CAL:
        {
            rc = cape_write_calibration();
            break;
        }

        case OP_RESET:
        {
            rc = cape_reset();
            break;
        }
        
        case OP_UPLOAD:
        {
            rc = boot_upload();
            break;
        }
        
        case OP_SET_I2C:
        {
            rc = cape_set_address( new_address );
            break;
        }

        case OP_VALUE:
        {
            rc = cape_show_value();
            break;
        }
        
        case OP_EXT_POWER:
        {
            rc = set_external_power();
            break;
        }

        default:
        case OP_NONE:
        {
            break;
        }
    }

    close( handle );
    return rc;
}
