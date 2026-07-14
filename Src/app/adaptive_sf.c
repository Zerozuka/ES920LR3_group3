/*******************************************************************************
 * adaptive_sf file.
 *
 * Adaptive spreading-factor control loop (policy only). See adaptive_sf.h for
 * the rationale, the LoRa-orthogonality escape idea, and the link-symmetry
 * caveat.
 *
 * (c) Copyright 2021, EASEL, Inc.  All rights reserved.
 *
 * No part of this document may be reproduced in any form - including copied,
 * transcribed, printed or by any electronic means - without specific written
 * permission from EASEL.
 *
 *******************************************************************************/

#include "app/adaptive_sf.h"

#if defined(BLD_ENABLE_ADAPTIVE_SF)

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

// Congestion / clear thresholds precomputed in the half-dBm magnitude domain.
// Remember: larger magnitude == weaker interference == quieter channel.
//   BUSY_MAG  : magnitude AT OR BELOW this => interference stronger than
//               ADAPTIVE_SF_BUSY_DBM  => congested.
//   QUIET_MAG : magnitude AT OR ABOVE this => interference weaker  than
//               ADAPTIVE_SF_QUIET_DBM => clear.
// QUIET_MAG > BUSY_MAG by construction, so the gap between them is the
// hysteresis band.
#define BUSY_MAG (ADAPTIVE_SF_MAG(ADAPTIVE_SF_BUSY_DBM))
#define QUIET_MAG (ADAPTIVE_SF_MAG(ADAPTIVE_SF_QUIET_DBM))

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/

static uint8_t gCurrentSf = ADAPTIVE_SF_FAST;

// Integer EWMA accumulator holding the smoothed magnitude scaled by
// 2^ADAPTIVE_SF_EWMA_SHIFT. The smoothed magnitude is gEwmaAcc >> SHIFT.
static uint32_t gEwmaAcc = 0;
static bool_t gSeeded = FALSE;

// Consecutive-sample counters used to debounce a switch decision.
static uint8_t gBusyDwell = 0;  // sustained congestion while on the fast SF
static uint8_t gQuietDwell = 0; // sustained clear channel while on the robust SF

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
 * AdaptiveSf_Init
 *******************************************************************************/
void AdaptiveSf_Init(uint8_t currentSf) {
  gCurrentSf = currentSf;
  gEwmaAcc = 0;
  gSeeded = FALSE;
  gBusyDwell = 0;
  gQuietDwell = 0;
}

/*******************************************************************************
 * AdaptiveSf_Update
 *******************************************************************************/
bool_t AdaptiveSf_Update(uint8_t rssiMag, uint8_t *outSf) {
  bool_t changed = FALSE;
  uint16_t smoothed;

  // Seed the filter on the first sample so it starts from the real floor
  // instead of ramping up from zero.
  if (!gSeeded) {
    gEwmaAcc = (uint32_t)rssiMag << ADAPTIVE_SF_EWMA_SHIFT;
    gSeeded = TRUE;
  } else {
    // EWMA: acc += sample - (acc >> SHIFT); smoothed == acc >> SHIFT.
    gEwmaAcc += (uint32_t)rssiMag - (gEwmaAcc >> ADAPTIVE_SF_EWMA_SHIFT);
  }

  smoothed = (uint16_t)(gEwmaAcc >> ADAPTIVE_SF_EWMA_SHIFT);

  if (gCurrentSf == ADAPTIVE_SF_FAST) {
    // On the fast SF: escape to the robust SF once the channel has been
    // congested for long enough.
    if (smoothed <= BUSY_MAG) {
      if (++gBusyDwell >= ADAPTIVE_SF_DWELL) {
        gCurrentSf = ADAPTIVE_SF_ROBUST;
        gBusyDwell = 0;
        gQuietDwell = 0;
        changed = TRUE;
      }
    } else {
      gBusyDwell = 0;
    }
  } else {
    // On the robust SF: fall back to the fast SF once the channel has been
    // clear for long enough.
    if (smoothed >= QUIET_MAG) {
      if (++gQuietDwell >= ADAPTIVE_SF_DWELL) {
        gCurrentSf = ADAPTIVE_SF_FAST;
        gBusyDwell = 0;
        gQuietDwell = 0;
        changed = TRUE;
      }
    } else {
      gQuietDwell = 0;
    }
  }

  *outSf = gCurrentSf;
  return changed;
}

/*******************************************************************************
 * AdaptiveSf_CurrentSf
 *******************************************************************************/
uint8_t AdaptiveSf_CurrentSf(void) { return gCurrentSf; }

/*******************************************************************************
 * AdaptiveSf_SmoothedMag
 *******************************************************************************/
uint16_t AdaptiveSf_SmoothedMag(void) {
  if (!gSeeded) {
    return 0;
  }
  return (uint16_t)(gEwmaAcc >> ADAPTIVE_SF_EWMA_SHIFT);
}

#endif /* BLD_ENABLE_ADAPTIVE_SF */
