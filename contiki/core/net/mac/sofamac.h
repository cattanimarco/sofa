/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: sofamac.h,v 1.5 2010/02/23 20:09:11 nifi Exp $
 */

/**
 * \file
 *         SOFA MAC protocol
 * \author
 *         Marco Cattani <m.cattani@gmail.com>
 */

#ifndef __SOFAMAC_H__
#define __SOFAMAC_H__

#include "sys/rtimer.h"
#include "sys/energest.h"
#include "net/mac/rdc.h"
#include "dev/radio.h"
#include "net/rime/rimeaddr.h"


//TODO: is this needed???
/*
#define SOFAMAC_RECEIVER "sofamac.recv"
#define SOFAMAC_STROBES "sofamac.strobes"
#define SOFAMAC_SEND_WITH_ACK "sofamac.send.ack"
#define SOFAMAC_SEND_WITH_NOACK "sofamac.send.noack"
*/




// SOFA FSM
enum mac_state{
disabled = 0,
enabled = 1,
idle = 1,
wait_master_packet = 2,
wait_slave_packet = 3,
wait_master_packacket_ack = 4,
wait_slave_strobe_ack = 5,
wait_to_send = 6
} ;

// SOFA's message structures
struct sofa_hdr {
  uint8_t type;
  uint16_t delay;
};

struct sofa_data_hdr {
  uint8_t type;
  uint16_t data;
  rimeaddr_t dst;
};

// SOFA communications outcomes
#define SOFA_SUCCESS 1
#define SOFA_ERROR 2
#define SOFA_BUSY 3

// SOFA's message types
#define DISPATCH          0
#define TYPE_STROBE       1
#define TYPE_DATA_M       2
#define TYPE_DATA_S       3
#define TYPE_STROBE_ACK   4
#define TYPE_DATA_ACK     5

// SOFA PARAMETERS
#define MAX_STROBE_SIZE 50
#define DEFAULT_PERIOD (RTIMER_ARCH_SECOND)
#define DEFAULT_ON_TIME (RTIMER_ARCH_SECOND / 200)
#define DEFAULT_OFF_TIME (DEFAULT_PERIOD - DEFAULT_ON_TIME)
#define DEFAULT_STROBE_WAIT_TIME DEFAULT_ON_TIME
#define DEFAULT_STROBE_TIME DEFAULT_ON_TIME + DEFAULT_OFF_TIME
#define MASTER_PACKET_WAITING_TIME (5 * DEFAULT_ON_TIME)
#define MASTER_ACK_WAITING_TIME (5 * DEFAULT_ON_TIME)
#define SLAVE_PACKET_WAITING_TIME (5 * DEFAULT_ON_TIME)
#define MASTER_BACKOFF_WAITING_TIME DEFAULT_ON_TIME*2

/* On some platforms, we may end up with a DEFAULT_PERIOD that is 0
   which will make compilation fail due to a modulo operation in the
   code. To ensure that DEFAULT_PERIOD is greater than zero, we use
   the construct below. */
#if DEFAULT_PERIOD == 0
#undef DEFAULT_PERIOD
#define DEFAULT_PERIOD 1
#endif

/*---------------------------------------------------------------------------*/
struct sofamac_config {
  rtimer_clock_t on_time;
  rtimer_clock_t off_time;
  rtimer_clock_t strobe_time;
  rtimer_clock_t strobe_wait_time;
};

struct sofa_callback{
void(* recv)(uint16_t rx_value);
uint16_t (* pull_val)(void);
void (* result)(uint16_t ret_value);
};


extern const struct rdc_driver sofamac_driver;
int send_packet(void);
void sofa_register(struct sofa_callback *);
void sofamac_tx(uint16_t);

#endif /* __SOFAMAC_H__ */
