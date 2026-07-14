/*******************************************************************************
* usr_util header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#ifndef _USR_UTIL_H_
#define _USR_UTIL_H_


/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/
#define isAsciiHex(ch)        ( ('0' <= (ch) && (ch) <= '9') || \
                              ('a' <= (ch) && (ch) <= 'f') || \
                              ('A' <= (ch) && (ch) <= 'F') )

#define isAsciiDec(ch)      ('0' <= (ch) && (ch) <= '9')

#define isAsciiOct(ch)      ('0' <= (ch) && (ch) <= '7')

#define isAsciiSpace(ch)    ((ch) == ' ' || (ch) == '\t' )

#define isAsciiPrintable(ch)    (' ' <= (ch) && (ch) <= '~')

#define AsciiToHex(ch)      ( ('0' <= (ch) && (ch) <= '9') ? ((ch) - '0')       :   \
                              ('a' <= (ch) && (ch) <= 'f') ? ((ch) - 'a' + 0xA) :   \
                              ('A' <= (ch) && (ch) <= 'F') ? ((ch) - 'A' + 0xA) : 0 )

#define AsciiToDec(ch)      ( ('0' <= (ch) && (ch) <= '9') ? ((ch) - '0')       : 0 )

#define AsciiToOct(ch)      ( ('0' <= (ch) && (ch) <= '7') ? ((ch) - '0')       : 0 )

#define div_ceil(a, b)  (((a)+(b)-1)/(b))
#define div_round(a, b) ((a)/(b) + (((a)%(b))*10/(b)+5)/10)
#define div_floor(a, b) ((a)/(b))

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

bool_t StrToHex16(const uint8_t* str, uint16_t* val, uint8_t width);
bool_t StrToHex(const uint8_t* str, uint8_t* dest, uint8_t length);
bool_t StrToUint8(const uint8_t* str, uint8_t* val, uint8_t width);
bool_t StrToUint32(const uint8_t* str, uint32_t* val, uint8_t width);
bool_t StrToInt32(const uint8_t* str, int32_t* val, uint8_t width);

int usr_vsnprintf(char* s, size_t n, const char* format, va_list arg);

#endif /* _USR_UTIL_H_ */
