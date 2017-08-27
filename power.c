#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <endian.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include "regs.h"

#define STM_ADDRESS         0x60
#define INA_ADDRESS         0x40

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;

typedef enum
{
    OP_NONE,
    OP_CHARGE,
    OP_EEPROM,
    OP_SET_TIMEOUT,
    OP_READ_RTC,
    OP_SET_SYSTIME,
    OP_WRITE_RTC,
    OP_INFO
} op_type;

op_type operation = OP_NONE;


int i2c_bus = 1;
int stm_address = STM_ADDRESS;
int charge_rate = 1;
int power_timeout = 0;
int handle;


void msleep ( int msecs )
{
    usleep ( msecs * 1000 );
}


int i2c_read ( void *buf, int len )
{
    int rc = 0;

    if ( read ( handle, buf, len ) != len )
    {
        printf ( "I2C read failed: %s\n", strerror ( errno ) );
        rc = -1;
    }

    return rc;
}


int i2c_write( void *buf, int len )
{
    int rc = 0;

    if ( write( handle, buf, len ) != len )
    {
        printf( "I2C write failed: %s\n", strerror ( errno ) );
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


int command_wait( uint8_t command )
{
    uint8_t r = 0xEE;
    int rc = -1;
    
    if ( register_write( REG_COMMAND, command ) == 0 )
    {
        do {
            msleep( 100 );
            if ( register_read( REG_COMMAND, &r ) != 0 )
                break;
        } while ( r == command );
        
        if ( r == 0 )
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


int cape_read_rtc( time_t *iptr )
{
    int rc = 1;
    uint32_t seconds;

    if ( command_read32( COMMAND_READ_RTC, &seconds ) == 0 )
    {
        printf( "Cape RTC seconds %08X (%d)\n", seconds, seconds );
        printf( ctime( (time_t*)&seconds ) );

        if ( iptr != NULL )
        {
            *iptr = seconds;
        }
        rc = 0;
    }
    else fprintf( stderr, "Error reading board RTC\n" );

    return rc;
}


int cape_write_rtc( void )
{
    int rc = 1;
    unsigned int seconds = time( NULL );

    printf( "System seconds %08X (%d)\n", seconds, seconds );
    printf( ctime( (time_t*)&seconds ) );

    if ( command_write32( COMMAND_WRITE_RTC, seconds ) == 0 )
    {
        rc = 0;
    }
    else fprintf( stderr, "Error writing board RTC\n" );

    return rc;
}


int cape_show_cape_info( void )
{
    uint8_t c;
    uint8_t product, step, revision;
    uint8_t ver_maj, ver_min;
    uint32_t time;
    
    if ( register_read( REG_PROD, &product ) == 0 )
    {
        printf( "\nProduct   : " );
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
        printf( "Revision  : " );
        if ( step == 0 ) printf( "P" );
        else printf( "%c", '@'+step );
        printf( "%c", '0'+revision );
        printf( "\n" );
    }
    else return -1;
    
    if ( register_read( REG_VERSION_MAJOR, &ver_maj ) != 0 )
        return -1;
    if ( register_read( REG_VERSION_MINOR, &ver_min ) != 0 )
        return -1;
    printf( "Interface : v%d.%d\n", ver_maj, ver_min );
    
    if ( command_read32( COMMAND_FIRMWARE_TIMESTAMP, &time ) == 0 )
    {
        printf( "Firmware  : %s", ctime( (time_t*)&time ) );
    }
    else return -1;    
    
    if ( command_read32( COMMAND_GET_ONTIME, &time ) == 0 )
    {
        printf( "On time   : %d\n", time );
    }
    else return -1;    
    
    if ( command_read32( COMMAND_GET_OFFTIME, &time ) == 0 )
    {
        printf( "Off time  : %d\n", time );
    }
    else return -1;    

    if ( register_read( REG_START_REASON, &c ) == 0 )
    {
        printf( "Powered on triggered by " );
        if ( c & START_BUTTON ) printf( "button press " );
        if ( c & START_EXTERNAL ) printf( "external event " );
        if ( c & START_PWRGOOD ) printf( "power good " );
        if ( c & START_TIMEOUT ) printf( "timer" );
        if ( c & START_PWR_ON ) printf( "inital power" );
        if ( c & START_WDT_RESET ) printf( "watchdog reset" );
        printf ( "\n\n" );
    }
    else return -1;
    
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


void show_usage( char *progname )
{
    fprintf( stderr, "Usage: %s [OPTION] \n", progname );
    fprintf( stderr, "   Options:\n" );
    fprintf( stderr, "      -h --help           Show usage.\n" );
    fprintf( stderr, "      -a --address <addr> Use I2C <addr> instead of 0x%02X.\n", STM_ADDRESS );
    fprintf( stderr, "      -c --charge <1-3>   Set charge rate in thirds of an amp.\n" );
    fprintf( stderr, "      -e --eeprom         Store current setting in EEPROM.\n" );
    fprintf( stderr, "      -i --info           Show board info.\n" );
    fprintf( stderr, "      -t --timeout        Set power-on timeout value.\n" );
    fprintf( stderr, "      -r --read           Read and display board RTC value.\n" );
    fprintf( stderr, "      -s --set            Set system time from RTC.\n" );
    fprintf( stderr, "      -w --write          Write RTC from system time.\n" );
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
            { "address",    1,  NULL,   'a'   },
            { "charge",     1,  NULL,   'c'   },
            { "eeprom",     0,  NULL,   'e'   },
            { "info",       0,  NULL,   'i'   },
            { "timeout",    1,  NULL,   't'   },
            { "read",       0,  NULL,   'r'   },
            { "set",        0,  NULL,   's'   },
            { "write",      0,  NULL,   'w'   },
            { NULL,         0,  NULL,    0    },
        };
        int c;

        c = getopt_long( argc, argv, "ha:c:it:rsw", lopts, NULL );

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
            
            case 'c':
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
            
            case 'e':
            {
                operation = OP_EEPROM;
                break;
            }
            
            case 'i':
            {
                operation = OP_INFO;
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

            case 's':
            {
                operation = OP_SET_SYSTIME;
                break;
            }

            case 'w':
            {
                operation = OP_WRITE_RTC;
                break;
            }

            case 'h':
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

    if ( verify_product() )
    {
        printf( "Board found at address 0x%X\n", stm_address );
    }
    else
    {
        close( handle );
        fprintf( stderr, "No board found at 0x%X\n", stm_address );
        exit( 1 );
    }
    
    switch ( operation )
    {
        case OP_INFO:
        {
            rc = cape_show_cape_info();
            break;
        }

        case OP_CHARGE:
        {
            rc = cape_set_charge_rate();
            break;
        }
        
        case OP_SET_TIMEOUT:
        {
            rc = cape_write_timeout();
            break;
        }
        
        case OP_READ_RTC:
        {
            rc = cape_read_rtc( NULL );
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

        default:
        case OP_NONE:
        {
            break;
        }
    }

    close( handle );
    return rc;
}
