/*******************************************************************************
* usr_util file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "app/usr_util.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/
static int usr_snprintf_char( char* s, size_t n, int arg, int width, bool_t left );
static int usr_snprintf_str( char* s, size_t n, const char* arg, int width, bool_t left );
static int usr_snprintf_dec( char* s, size_t n, int arg, int width, bool_t left, bool_t zero );
static int usr_snprintf_udec( char* s, size_t n, unsigned arg, int width, bool_t left, bool_t zero );
static int usr_snprintf_hex( char* s, size_t n, unsigned arg, int width, bool_t left, bool_t zero, bool_t capital );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
* StrToHex16
*
* Interface assumptions:
*     str           string
*     val           value
*
* Return value:
*     bool_t        result (0: failed, 1:succeed)
*
*******************************************************************************/
bool_t StrToHex16( const uint8_t* str, uint16_t* val, uint8_t width )
{
    uint8_t     findIndex;

    *val = 0;

    for( findIndex = 0; findIndex < width; findIndex++ )
    {
        if( isAsciiHex(str[findIndex]) )
        {
            *val |= AsciiToHex( str[findIndex] );
        }
        else
        {
            return FALSE;
        }

        if( '\0' == str[findIndex+1] )
        {
            return TRUE;
        }

        *val <<= 4;
    }

    return FALSE;
}

/*******************************************************************************
* StrToHex
*
* Interface assumptions:
*     str           string
*     val           value
*
* Return value:
*     bool_t        result (0: failed, 1:succeed)
*
*******************************************************************************/
bool_t StrToHex( const uint8_t* str, uint8_t* dest, uint8_t length )
{
    uint8_t     findIndex;
    uint8_t     i = 0;

    if( length == 0 )
    {
        return TRUE;
    }

    memset( dest, 0, length );

    // seek to tail
    for( findIndex = 0; findIndex < (length * 2); findIndex++ )
    {
        if( '\0' == str[findIndex] )
        {
            break;
        }

        if( !isAsciiHex(str[findIndex]) )
        {
            return FALSE;
        }
    }
    if( findIndex == 0 || str[findIndex] != '\0' )
    {
        return FALSE;
    }

    for( i = (length - 1); ; i--)
    {
        // lower 4bit
        findIndex--;
        dest[i] = AsciiToHex( str[findIndex] );
        if( 0 == findIndex )
        {
            break;
        }

        // upper 4bit
        findIndex--;
        dest[i] |= AsciiToHex( str[findIndex] ) << 4;
        if( 0 == findIndex )
        {
            break;
        }

        if( i == 0 )
        {
            break;
        }
    }

    return TRUE;
}

/*******************************************************************************
* StrToUint8
*
* Interface assumptions:
*     str           string
*     val           value
*     width         width
*
* Return value:
*     bool_t        result (0: failed, 1:succeed)
*
*******************************************************************************/
bool_t StrToUint8( const uint8_t* str, uint8_t* val, uint8_t width )
{
    uint8_t     findIndex;

    *val = 0;

    for( findIndex = 0; findIndex < width; findIndex++ )
    {
        if( isAsciiDec(str[findIndex]) )
        {
            *val += AsciiToDec( str[findIndex] );
        }
        else
        {
            return FALSE;
        }

        if( '\0' == str[findIndex+1] )
        {
            return TRUE;
        }

        *val *= 10;
    }

    return FALSE;
}

/*******************************************************************************
* StrToUint32
*
* Interface assumptions:
*     str           string
*     val           value
*     width         width
*
* Return value:
*     bool_t        result (0: failed, 1:succeed)
*
*******************************************************************************/
bool_t StrToUint32( const uint8_t* str, uint32_t* val, uint8_t width )
{
    uint8_t     findIndex;

    *val = 0;

    for( findIndex = 0; findIndex < width; findIndex++ )
    {
        if( isAsciiDec(str[findIndex]) )
        {
            *val += AsciiToDec( str[findIndex] );
        }
        else
        {
            return FALSE;
        }

        if( '\0' == str[findIndex+1] )
        {
            return TRUE;
        }

        *val *= 10;
    }

    return FALSE;
}

/*******************************************************************************
* StrToInt32
*
* Interface assumptions:
*     str           string
*     val           value
*     width         width
*
* Return value:
*     bool_t        result (0: failed, 1:succeed)
*
*******************************************************************************/
bool_t StrToInt32( const uint8_t* str, int32_t* val, uint8_t width )
{
    uint32_t tmpVal;

    if( width == 0 )
    {
        return FALSE;
    }

    if( str[0] == '-' )
    {
        if( !StrToUint32(str + 1, &tmpVal, width - 1) )
        {
            return FALSE;
        }
        *val = -(int32_t)tmpVal;
    }
    else
    {
        if( !StrToUint32(str, &tmpVal, width) )
        {
            return FALSE;
        }
        *val = tmpVal;
    }

    return TRUE;
}

/*******************************************************************************
* usr_vsnprintf
*
* Interface assumptions:
*     s             output string
*     n             max length of output string
*     format        format string
*     arg           arguments
*
* Return value:
*     int           length of output string
*
*******************************************************************************/
int usr_vsnprintf( char* s, size_t n, const char* format, va_list arg )
{
    int cnt = 0;
    char ch;

    if( n == 0 ) { return 0; }

    for( ch = *format++; ch != '\0'; )
    {
        bool_t zero = FALSE;
        bool_t left = FALSE;
        int width = 0;

        if( cnt + 1 >= n )
        {
            break;
        }

        switch( ch )
        {
        case '%':
            ch = *format++;

            // %%
            //  ~
            if( ch == '%' )
            {
                s[cnt++] = ch;
                ch = *format++;
                break;
            }

            // %-0123d
            //  ~
            if( ch == '-' )
            {
                left = TRUE;
                ch = *format++;
            }

            // %-0123d
            //   ~
            if( ch == '0' )
            {
                zero = TRUE;
                ch = *format++;
            }

            // %-0123d
            //    ~~~
            while( isAsciiDec(ch) )
            {
                width *= 10;
                width += AsciiToDec(ch);
                ch = *format++;
            }

            // %-0123d
            //       ~
            switch( ch )
            {
            case 'c':
                cnt += usr_snprintf_char( s + cnt, n - cnt - 1, va_arg(arg, int), width, left );
                break;

            case 's':
                cnt += usr_snprintf_str( s + cnt, n - cnt - 1, va_arg(arg, char*), width, left );
                break;

            case 'd':
                cnt += usr_snprintf_dec( s + cnt, n - cnt - 1, va_arg(arg,int), width, left, zero );
                break;

            case 'u':
                cnt += usr_snprintf_udec( s + cnt, n - cnt - 1, va_arg(arg,unsigned), width, left, zero );
                break;

            case 'x':
                cnt += usr_snprintf_hex( s + cnt, n - cnt - 1, va_arg(arg,unsigned), width, left, zero, FALSE );
                break;

            case 'X':
                cnt += usr_snprintf_hex( s + cnt, n - cnt - 1, va_arg(arg,unsigned), width, left, zero, TRUE );
                break;

            default:
                break;
            }
            ch = *format++;
            break;

        default:
            s[cnt++] = ch;
            ch = *format++;
            break;
        }
    }
    s[cnt] = '\0';

    return cnt;
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static int usr_snprintf_char( char* s, size_t n, int arg, int width, bool_t left )
{
    int cnt = 0;
    const int len = 1;
    int i;

    if( n == 0 )
    {
        return 0;
    }

    // fill space
    if( !left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    // fill char
    s[cnt++] = (char)arg;
    if( cnt >= n )
    {
        return cnt;
    }

    // fill space
    if( left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    return cnt;
}

static int usr_snprintf_str( char* s, size_t n, const char* arg, int width, bool_t left )
{
    int cnt = 0;
    int len = strlen(arg);
    int i;

    if( n == 0 )
    {
        return 0;
    }

    // fill space
    if( !left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    // fill string
    if( cnt + len < n )
    {
        memcpy( s + cnt, arg, len );
        cnt += len;
    }
    else
    {
        memcpy( s + cnt, arg, n - cnt );
        return n;
    }

    // fill space
    if( left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    return cnt;
}

static int usr_snprintf_dec( char* s, size_t n, int arg, int width, bool_t left, bool_t zero )
{
    int cnt = 0;
    int len;
    int i;
    bool_t minus = FALSE;
    unsigned base;
    unsigned abs;
    unsigned tmp;

    if( n == 0 )
    {
        return 0;
    }

    if( arg < 0 )
    {
        minus = TRUE;
        abs = (unsigned)-arg;
    }
    else
    {
        minus = FALSE;
        abs = (unsigned)arg;
    }

    // calc length
    len = 1;
    base = 1;
    for( tmp = abs; tmp >= 10; tmp /= 10 )
    {
        len++;
        base*=10;
    }
    if( minus )
    {
        len++;
    }

    // fill space
    if( !left && !zero )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    // fill minus
    if( minus )
    {
        s[cnt++] = '-';
        if( cnt >= n )
        {
            return cnt;
        }
    }

    // fill zero
    if( !left && zero )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = '0';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    // fill decimal
    for( ; base; base /= 10 )
    {
        s[cnt++] = '0' + (abs/base)%10;
        if( cnt >= n )
        {
            return cnt;
        }
    }

    // fill space
    if( left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    return cnt;
}

static int usr_snprintf_udec( char* s, size_t n, unsigned arg, int width, bool_t left, bool_t zero )
{
    int cnt = 0;
    int len;
    int i;
    unsigned base;
    unsigned tmp;

    if( n == 0 )
    {
        return 0;
    }

    // calc length
    len = 1;
    base = 1;
    for( tmp = arg; tmp >= 10; tmp /= 10 )
    {
        len++;
        base*=10;
    }

    // fill space or zero
    if( !left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = zero ? '0' : ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    // fill decimal
    for( ; base; base /= 10 )
    {
        s[cnt++] = '0' + (arg/base)%10;
        if( cnt >= n )
        {
            return cnt;
        }
    }

    // fill space
    if( left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    return cnt;
}

static int usr_snprintf_hex( char* s, size_t n, unsigned arg, int width, bool_t left, bool_t zero, bool_t capital )
{
    int cnt = 0;
    int len;
    int i;
    unsigned base;
    unsigned tmp;

    if( n == 0 )
    {
        return 0;
    }

    // calc length
    len = 1;
    base = 1;
    for( tmp = arg; tmp >= 0x10; tmp /= 0x10 )
    {
        len++;
        base*=0x10;
    }

    // fill space or zero
    if( !left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = zero ? '0' : ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    // fill decimal
    for( ; base; base /= 0x10 )
    {
        tmp = (arg/base)%0x10;
        s[cnt++] = ( (tmp < 10) ? '0'       :
                     capital    ? ('A'-10)  :
                                  ('a'-10)  ) + tmp;
        if( cnt >= n )
        {
            return cnt;
        }
    }

    // fill space
    if( left )
    {
        for( i = 0; i + len < width; i++ )
        {
            s[cnt++] = ' ';
            if( cnt >= n )
            {
                return cnt;
            }
        }
    }

    return cnt;
}
