/*******************************************************************************
 * adaptive_sf header file.
 *
 * Adaptive spreading-factor control loop.
 *
 * The loop watches the surrounding interference (the RF noise / rival-traffic
 * floor sampled while the radio is idle in RX) and dynamically hops the LoRa
 * spreading factor between a "fast" SF (SF7) and a "robust" SF (SF10).
 *
 * LoRa spreading factors are quasi-orthogonal: a receiver locked to one SF
 * treats energy from a different SF as noise and largely rejects it. When a
 * channel is crowded by rival waves transmitting at the fast SF, hopping to a
 * higher SF lets this link "slip through" the congestion on an orthogonal
 * code, at the cost of longer airtime. When the channel clears again the loop
 * drops back to the fast SF to recover throughput and duty-cycle headroom.
 *
 * IMPORTANT (link symmetry): SF is a shared modem parameter. Two endpoints can
 * only demodulate each other while they use the SAME SF. This module only
 * decides the local SF from the local interference estimate. For a link to
 * stay up, BOTH endpoints must run this policy so that, observing the same
 * congestion, they converge to the same SF. This mirrors how symmetric ADR is
 * expected to be deployed.
 *
 * This module is pure policy: it owns the decision state (smoothing,
 * hysteresis, dwell) and returns whether the SF should change. Applying the
 * new SF to the radio and re-arming RX is the caller's responsibility.
 *
 * (c) Copyright 2021, EASEL, Inc.  All rights reserved.
 *
 * No part of this document may be reproduced in any form - including copied,
 * transcribed, printed or by any electronic means - without specific written
 * permission from EASEL.
 *
 *******************************************************************************/

#ifndef __ADAPTIVE_SF_H
#define __ADAPTIVE_SF_H

#include "config/usr_config.h"

#if defined(BLD_ENABLE_ADAPTIVE_SF)

#include "usr_common.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

// Spreading factors the loop hops between.
//   FAST   : short airtime, higher data rate, weaker interference immunity.
//   ROBUST : long airtime, stronger processing gain, orthogonal escape route.
#define ADAPTIVE_SF_FAST (7)   // SF7
#define ADAPTIVE_SF_ROBUST (10) // SF10

// Interference thresholds expressed in dBm (the natural unit for a noise
// floor). The band between QUIET and BUSY is the hysteresis gap that stops the
// loop from flapping around a single threshold.
//   BUSY  : interference stronger than this => channel is congested.
//   QUIET : interference weaker  than this => channel is clear.
#define ADAPTIVE_SF_BUSY_DBM (-85)
#define ADAPTIVE_SF_QUIET_DBM (-95)

// SMAC_GetRssi() reports the RSSI magnitude in half-dBm steps, i.e. the
// returned value equals -2 * (RSSI in dBm). A larger value therefore means a
// WEAKER signal (a quieter channel). This macro maps a dBm threshold into that
// same magnitude domain so all comparisons stay in integer half-dBm units.
#define ADAPTIVE_SF_MAG(dbm) ((uint16_t)(-2 * (dbm)))

// Exponential moving-average smoothing of the raw RSSI samples.
// Smoothed value follows alpha = 1 / 2^SHIFT (SHIFT = 3 => alpha = 1/8).
#define ADAPTIVE_SF_EWMA_SHIFT (3)

// Number of consecutive smoothed samples that must satisfy the switch
// condition before the SF actually changes (debounce / dwell time). At a
// 200 ms sample period, 8 samples is ~1.6 s of sustained condition.
#define ADAPTIVE_SF_DWELL (8)

// Sample period of the control loop in milliseconds.
#define ADAPTIVE_SF_SAMPLE_MS (200)

/*******************************************************************************
********************************************************************************
* Public prototypes
********************************************************************************
*******************************************************************************/

/*******************************************************************************
 * AdaptiveSf_Init
 *
 * Reset the control loop and seed it with the SF currently in use.
 *
 * Interface assumptions:
 *     currentSf   the SF the radio is configured with right now
 *
 * Return value:
 *     None
 *******************************************************************************/
void AdaptiveSf_Init(uint8_t currentSf);

/*******************************************************************************
 * AdaptiveSf_Update
 *
 * Feed one interference sample into the loop and run one control step.
 *
 * Interface assumptions:
 *     rssiMag     RSSI magnitude as returned by SMAC_GetRssi()
 *                 (half-dBm units, larger == weaker == quieter)
 *     outSf       [out] the SF to use going forward (never NULL)
 *
 * Return value:
 *     TRUE  if the SF changed this step (caller must apply *outSf and re-arm)
 *     FALSE otherwise
 *******************************************************************************/
bool_t AdaptiveSf_Update(uint8_t rssiMag, uint8_t *outSf);

/*******************************************************************************
 * AdaptiveSf_CurrentSf
 *
 * Return value:
 *     The SF the loop currently believes should be in use.
 *******************************************************************************/
uint8_t AdaptiveSf_CurrentSf(void);

/*******************************************************************************
 * AdaptiveSf_SmoothedMag
 *
 * Return value:
 *     The current smoothed interference magnitude (half-dBm units), for
 *     telemetry. Returns 0 before the first sample.
 *******************************************************************************/
uint16_t AdaptiveSf_SmoothedMag(void);

#endif /* BLD_ENABLE_ADAPTIVE_SF */

#endif /* __ADAPTIVE_SF_H */
