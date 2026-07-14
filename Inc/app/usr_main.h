/*******************************************************************************
* usr_main header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#ifndef _USR_MAIN_H_
#define _USR_MAIN_H_

#include "usart.h"
#include "wwdg.h"
#include "app/do_operation.h"
#include "app/do_configuration.h"

#ifdef __cplusplus
    extern "C" {
#endif


/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef enum ResetType_tag
{
    POR_RESET = 1,  // POR reset
    PIN_RESET = 2,  // PIN reset
    WDG_RESET = 3,  // WDG reset
    SOFT_RESET = 4, // Software reset
} ResetType_t;

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/
void UsrMain( void );
void MakeSmacParams( smacParams_t* params );
ResetType_t GetResetType( void );


#ifdef __cplusplus
}
#endif

/******************************************************************************/
#endif /* _USR_MAIN_H_ */
