/*******************************************************************************
* powerctl header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __POWERCTL_H
#define __POWERCTL_H
#ifdef __cplusplus
 extern "C" {
#endif

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/
void UsrPower_init( void );
void UsrPower_enterActiveRun( void );
void UsrPower_enterLowPowerRun( void );
uint32_t UsrPower_waitEventInStopMode( uint32_t (*GetEventRoutine)(void), uint32_t baudRate );
uint32_t UsrPower_waitEventInCpuSleep( uint32_t (*GetEventRoutine)(void) );

#ifdef __cplusplus
}
#endif

#endif /* __POWERCTL_H */

