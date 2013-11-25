#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "net/netstack.h"
#include "lib/random.h"
#include "net/mac/sofamac.h"
#include "net/rime.h"
#include "net/rime/timesynch.h"
#include "sys/compower.h"
#include "sys/pt.h"
#include "sys/rtimer.h"
#include "contiki-conf.h"
#ifdef EXPERIMENT_SETUP
#include "experiment-setup.h"
#endif
#include <string.h>
#include <stdio.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTDEBUG(...) printf(__VA_ARGS__)
#else
#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE
#define LEDS_ON(x)
#define LEDS_OFF(x)
#define LEDS_TOGGLE(x)
#define PRINTF(...)
#define PRINTDEBUG(...)
#endif

// SOFA OPTIONS
#define PUSHPULL 1
// retransmit beacon acknoledgments
#define RETX 1
/* before sending listen to the channel for a certain period */
#define USE_BACKOFF 1
// after the backoff, if we receive a beacon instead on starting a communication we go to sleep
#define SLEEP_BACKOFF 0

// MACROS
#define CSCHEDULE_POWERCYCLE(rtime) cschedule_powercycle((1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND)
#define GOTO_IDLE(rtime) ctimer_set(&idle_timeout_ctimer,(1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND,(void (*)(void *))goto_idle, NULL)
#define STOP_IDLE() ctimer_stop(&idle_timeout_ctimer)
#define SEND_BACKOFF(rtime) ctimer_set(&backoff_ctimer,(1ul * CLOCK_SECOND * (rtime)) / RTIMER_ARCH_SECOND,(void (*)(void *))send_packet, NULL)
#define STOP_BACKOFF() ctimer_stop(&backoff_ctimer)

#ifndef MIN
#define MIN(a, b) ((a) < (b)? (a) : (b))
#endif /* MIN */

// GLOBAL VARIABLES

//value to be sent
uint16_t sofa_value;
// Retransmission probability
#if RETX
int p_retx = 50;
#endif
// Timers
static struct ctimer idle_timeout_ctimer;
static struct ctimer cpowercycle_ctimer;
static struct ctimer backoff_ctimer;
// FSM
enum mac_state current_state = disabled;
// MAC parameters
struct sofamac_config sofamac_config =
	{
		DEFAULT_ON_TIME, DEFAULT_OFF_TIME,
		DEFAULT_STROBE_TIME, DEFAULT_STROBE_WAIT_TIME
	};
static struct pt pt;
// callbacks
const struct sofa_callback *u;

/*---------------------------------------------------------------------------*/
static void powercycle_turn_radio_off(void)
    {
    if (current_state == idle)	NETSTACK_RADIO.off();
    }

/*---------------------------------------------------------------------------*/
static void powercycle_turn_radio_on(void)
    {
    if (current_state != disabled)
	{
	NETSTACK_RADIO.on();
#if USE_BACKOFF
	if (current_state == wait_to_send)
	    SEND_BACKOFF(MASTER_BACKOFF_WAITING_TIME);
	}
#endif
    }

/*---------------------------------------------------------------------------*/
static char cpowercycle(void *ptr);
static void cschedule_powercycle(clock_time_t time)
    {
    if (current_state != disabled)
	{
	if (time == 0) time = 1;
	ctimer_set(&cpowercycle_ctimer, time, (void (*)(void *)) cpowercycle, NULL);
	}
    }

/*---------------------------------------------------------------------------*/
static void goto_idle()
    {
    current_state = idle;
    powercycle_turn_radio_off();
    }

/*---------------------------------------------------------------------------*/
static char cpowercycle(void *ptr)
    {
    PT_BEGIN (&pt)
			    ;
    while (1)
	{
	powercycle_turn_radio_on();
	CSCHEDULE_POWERCYCLE(DEFAULT_ON_TIME);
	PT_YIELD(&pt);
	powercycle_turn_radio_off();
	CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
	PT_YIELD(&pt);
	}
    PT_END (&pt);
    }

/*---------------------------------------------------------------------------*/
int send_packet(void)
    {
    int len, strobe_len, data_len, strobes, collisions;
    struct sofa_hdr *hdr;
    struct sofa_data_hdr *data_pkt;
    uint8_t strobe[MAX_STROBE_SIZE];
    uint8_t data[MAX_STROBE_SIZE];
    rtimer_clock_t t0, t;
    rimeaddr_t strobe_ack_sender;

    //TODO: instead of enforcing broadcast, we should allow the application to also use unicast
    // ensure that the sender of the packet is this node and the receiver is null
    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &rimeaddr_null);
    // Create the sofamac header for the beacon
    len = NETSTACK_FRAMER.create();
    strobe_len = len + sizeof(struct sofa_hdr);
    if (len == 0 || strobe_len > (int) sizeof(strobe))
	{
	PRINTDEBUG("sofamac: data send failed, too large header\n");
	current_state = idle;
	return MAC_TX_ERR_FATAL;
	}
    // copy header to our structure
    memcpy(strobe, packetbuf_hdrptr(), len);
    // set packet type
    strobe[len] = TYPE_STROBE;
    // update FSM
    current_state = wait_slave_strobe_ack;
    // clear the receiver address
    rimeaddr_copy(&strobe_ack_sender, &rimeaddr_null);
    // Turn on the radio to listen for the strobe ACK
    powercycle_turn_radio_on();
    watchdog_stop();
    // clear the collision count
    collisions = 0;
    // Send a train of strobes until the receiver answers with an ACK
    t0 = RTIMER_NOW ();
    for (strobes = 0; current_state == wait_slave_strobe_ack && collisions == 0 && RTIMER_CLOCK_LT (RTIMER_NOW (), t0 + sofamac_config.strobe_time); strobes++)
	{
	t = RTIMER_NOW ();
	// listen for incoming ACKs
	while (current_state == wait_slave_strobe_ack && RTIMER_CLOCK_LT (RTIMER_NOW (),
		t + sofamac_config.strobe_wait_time))
	    {
	    // Check if we received an ACK
	    packetbuf_clear();
	    len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
	    if (len > 0)
		{
		packetbuf_set_datalen(len);
		if (NETSTACK_FRAMER.parse())
		    {

		    hdr = packetbuf_dataptr();
		    if (hdr->type == TYPE_STROBE_ACK)
			{
			if (rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_node_addr))
			    {
			    // save the address of the receiver
			    rimeaddr_copy(&strobe_ack_sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
			    // update FSM
			    current_state = wait_slave_packet;
			    }
			else // ACK not for us
			    {
			    PRINTDEBUG("sofamac: strobe ack for someone else\n");
			    }
			}
		    else // not a STROBE ACK
			{
			PRINTDEBUG(
				"sofamac: expected strobe ack, got data or other type \n");
			collisions++;
			}
		    }
		else // fail to parse the packet
		    {
		    PRINTDEBUG(
			    "sofamac: expected strobe ack, packet failed to parse %u\n", len);

		    }
		} // endif read packet length > 0
	    } // end strobe_wait_time
	//TODO: this sould not be needed
	t = RTIMER_NOW ();
	// Send the beacon
	if (current_state == wait_slave_strobe_ack && collisions == 0)
	    {
	    NETSTACK_RADIO.send(strobe, strobe_len);
	    }
	}
    // end of strobe sending time or got a collision or node is no more waiting for a strobe packet
    if (current_state == wait_slave_packet && collisions == 0)
	{
	packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &rimeaddr_null);
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
	// Create the data header for the data packet.
	len = NETSTACK_FRAMER.create();
	data_len = len + sizeof(struct sofa_data_hdr);
	if (len == 0 || data_len > (int) sizeof(data))
	    {
	    PRINTDEBUG("sofamac: data send failed, too large header\n");
	    return MAC_TX_ERR_FATAL;
	    }
	memcpy(data, packetbuf_hdrptr(), len);
	data_pkt = (struct sofa_data_hdr *)&(data[len]);
	data_pkt->type = TYPE_DATA_M;
	// this value is provided buy the application
	data_pkt->data = sofa_value;
	rimeaddr_copy(&(data_pkt->dst), &strobe_ack_sender);
	packetbuf_compact(); // This assures that the entire packet is consecutive in memory
	NETSTACK_RADIO.send(data, data_len);
	GOTO_IDLE(SLAVE_PACKET_WAITING_TIME);
	PRINTDEBUG(
		"sofamac: send data to %d,%d (strobes=%u,len=%u,%s), done\n", packetbuf_addr (PACKETBUF_ADDR_RECEIVER)->u8[0], packetbuf_addr (PACKETBUF_ADDR_RECEIVER)->u8[1], strobes, packetbuf_totlen (), current_state == wait_slave_packet ? "ack" : "no ack");
	}
    else // we got a collision or we didn't receive a strobe ack
	{
	current_state = idle;
	powercycle_turn_radio_off();
	}
    watchdog_start();
    if (collisions == 0)
	{
	if (current_state == wait_slave_strobe_ack)
	    {
	    return MAC_TX_NOACK;
	    }
	else
	    {
	    return MAC_TX_OK;
	    }
	}
    else
	{
	return MAC_TX_COLLISION;
	}
    }

/*---------------------------------------------------------------------------*/
static void qsend_packet(mac_callback_t sent, void *ptr)
    {
    int tx_status = MAC_TX_OK;

    if (current_state == idle)
	{
#if USE_BACKOFF    
	current_state = wait_to_send;
	powercycle_turn_radio_on();
#else
	powercycle_turn_radio_on();
	send_packet();
#endif
	}
    /*
 this function is in mac.c
 mac_call_sent_callback(mac_callback_t sent, void *ptr, int status, int num_tx)
 we should call it when all is finished.. after receiving the last ACK
     */
    mac_call_sent_callback(sent, ptr, tx_status, 1);
    }

/*---------------------------------------------------------------------------*/
static void input_packet(void)
    {
    struct sofa_hdr *hdr;
    struct sofa_data_hdr *data_pkt;
    uint8_t data[MAX_STROBE_SIZE];
    uint8_t ack[MAX_STROBE_SIZE];
    int len, data_len, ack_len;
    if (NETSTACK_FRAMER.parse())
	{
#if USE_BACKOFF
	//if we are waiting for sending something and we receive some packets, we stop the sending procedure (the channel is already busy. We switch to idle mode and act as a receiver. If SLEEP_BACKOFF is selected we go instead to sleep.
	if (current_state == wait_to_send)
	    {
	    STOP_BACKOFF ();
#if !SLEEP_BACKOFF
	    current_state = idle;
#else
	    if(u->result) u->result(SOFA_BUSY);
#endif
	    }
#endif

	hdr = packetbuf_dataptr();
	if (hdr->type == TYPE_DATA_S)
	    {
	    data_pkt = packetbuf_dataptr();
	    if (rimeaddr_cmp(&(data_pkt->dst), &rimeaddr_node_addr))
		{
		if (current_state == wait_slave_packet)
		    {
		    // stop timeout timer
		    STOP_IDLE ();
		    // send the received value to the application
			if(u->recv) {
    			u->recv(data_pkt->data);
  			}
		    PRINTDEBUG(
			    "sofamac: data(%d) from %d,%d \n", data_pkt->data, packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[0], packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[1]);
		    current_state = idle;
		    /* send the final ACK */
		    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, packetbuf_addr(PACKETBUF_ADDR_SENDER));
		    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
		    /* Create the ack header */
		    len = NETSTACK_FRAMER.create();
		    ack_len = len + sizeof(struct sofa_hdr);
		    if (len == 0 || ack_len > (int) sizeof(ack))
			{
			PRINTDEBUG("sofamac: data send failed, too large header\n");
			return;
			}
		    memcpy(ack, packetbuf_hdrptr(), len);
		    // set packet type
		    ack[len] = TYPE_DATA_ACK;
		    // this assures that the entire packet is consecutive in memory
		    packetbuf_compact();
		    NETSTACK_RADIO.send(ack, ack_len);
		    // notify the application that the data exchange was successfull
		    if(u->result) u->result(SOFA_SUCCESS);
		    current_state = idle;
		    powercycle_turn_radio_off();
		    return;
		    }
		else
		    {
		    //we are not in a state where we are expecting data
		    current_state = idle;
		    // notify the application that the data exchange was not successfull
		    if(u->result) u->result(SOFA_ERROR);
		    powercycle_turn_radio_off();
		    return;
		    }
		}
	    else
		{
		// the data packet is not for us
		current_state = idle;
		// notify the application that the data exchange was not successfull
		if(u->result) u->result(SOFA_ERROR);
		powercycle_turn_radio_off();
		PRINTDEBUG(
			"sofamac: data not for us (%d,%d->%d,%d)\n", packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[0], packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[1], packetbuf_addr (PACKETBUF_ADDR_RECEIVER)->u8[0], packetbuf_addr (PACKETBUF_ADDR_RECEIVER)->u8[1]);
		}
	    }
	else if (hdr->type == TYPE_DATA_M)
	    {
	    data_pkt = packetbuf_dataptr();
	    PRINTDEBUG(
		    "sofamac: master data from %d,%d. Destination %d.%d\n",
		    packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[0], packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[1],(data_pkt->dst).u8[0],(data_pkt->dst).u8[1]);
	    if (rimeaddr_cmp(&(data_pkt->dst), &rimeaddr_node_addr))
		{
		if (current_state == wait_master_packet)
		    {
		    rimeaddr_t dst;
		    rimeaddr_copy(&dst, packetbuf_addr(PACKETBUF_ADDR_SENDER));
		    // stop waiting timeout
		    STOP_IDLE ();
		    // send the received value to the application
			if(u->recv) {
    			u->recv(data_pkt->data);
  			}
		    // update FSM
		    current_state = wait_master_packacket_ack;
		    // send our data to the initiator
		    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &rimeaddr_null);
		    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
		    // Create the data header for the data packet
		    len = NETSTACK_FRAMER.create();
		    data_len = len + sizeof(struct sofa_data_hdr);
		    if (len == 0 || data_len > (int) sizeof(data))
			{
			PRINTDEBUG("sofamac: data send failed, too large header\n");
			return;
			}
		    memcpy(data, packetbuf_hdrptr(), len);
		    data_pkt = (struct sofa_data_hdr *)&(data[len]);
		    data_pkt->type = TYPE_DATA_S;
		    //ask the application for a value to send
			if(u->pull_val) {
    			data_pkt->data = u->pull_val();
  			}
else{
		    data_pkt->data = 0;
}
		    rimeaddr_copy(&(data_pkt->dst), &dst);
		    // This assures that the entire packet is consecutive in memory
		    packetbuf_compact();
		    NETSTACK_RADIO.send(data, data_len);
		    // set timeout for waiting the packet
		    GOTO_IDLE(MASTER_ACK_WAITING_TIME);
		    return;
		    }
		else
		    {
		    //we received a packet not for us
		    current_state = idle;
		    // notify the application that the data exchange was not successfull
		    if(u->result) u->result(SOFA_ERROR);
		    powercycle_turn_radio_off();
		    return;
		    }
		}
	    else
		{
		current_state = idle;
		powercycle_turn_radio_off();
		PRINTDEBUG(
			"sofamac: data not for us (%d,%d->%d,%d)\n", packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[0], packetbuf_addr (PACKETBUF_ADDR_SENDER)->u8[1], data_pkt->dst.u8[0], data_pkt->dst.u8[1]);
		return;

		}
	    }
	else if (hdr->type == TYPE_STROBE)
	    {
#if WITH_RETX
		if ((current_state == idle) || ((current_state == wait_master_packet) && ((random_rand()%100) < p_retx)))
#else
        	 if (current_state == idle)
#endif
		    {
		    if(current_state == wait_master_packet) PRINTDEBUG("ACK retransmitted\n");
		    hdr->type = TYPE_STROBE_ACK;
		    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, packetbuf_addr(PACKETBUF_ADDR_SENDER));
		    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
		    packetbuf_compact();
		    if (NETSTACK_FRAMER.create())
			{
			NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
			PRINTDEBUG(
				"sofamac: send strobe ack to %d,%d\n", packetbuf_addr (PACKETBUF_ADDR_RECEIVER)->u8[0], packetbuf_addr (PACKETBUF_ADDR_RECEIVER)->u8 [1]);
			current_state = wait_master_packet;
			// set a timeout for waiting data (sender can chose another node to send data)
			GOTO_IDLE(MASTER_PACKET_WAITING_TIME);
			return;
			}
		    else
			{
			PRINTDEBUG(
				"sofamac: failed to send strobe ack, going back to idle\n");
			current_state = idle;
			powercycle_turn_radio_off();
			return;
			}
		    }
		else
		    {
		    PRINTDEBUG("long sofa: stray strobe\n");
		    current_state = idle;
		    // since we are not in a idle state, we probably already sent an ack for a strobe. we then turn off our radio since the master is not interested in sending data to us
		    powercycle_turn_radio_off();
		    return;
		    }
	    }
	else if (hdr->type == TYPE_STROBE_ACK)
	    {
	    PRINTDEBUG("sofamac: stray strobe ack\n");
	    return;
	    }
	else if (hdr->type == TYPE_DATA_ACK)
	    {
	    if (current_state == wait_master_packacket_ack && rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_node_addr))
		{
		// stop waiting timeout
		STOP_IDLE ();
		PRINTF("sofamac: got data ack\n");
		current_state = idle;
		// notify the application that the data exchange was successfull
		if(u->result) u->result(SOFA_SUCCESS);
		powercycle_turn_radio_off();
		return;
		}
	    else
		{
		current_state = idle;
		// notify the application that the data exchange was not successfull
		if(u->result) u->result(SOFA_ERROR);
		powercycle_turn_radio_off();
		PRINTDEBUG("sofamac: stray data ack\n");
		return;
		}
	    }
	else
	    {
	    current_state = idle;
	    // notify the application that the data exchange was not successfull
	    if(u->result) u->result(SOFA_ERROR);
	    powercycle_turn_radio_off();
	    PRINTF(
		    "sofamac: unknown or unwanted packet type %u (%u)\n", hdr->type, packetbuf_datalen ());
	    return;
	    }
	}
    else
	{
	// now we go to sleep if the radio fails to parse the packet. We can also ignore the packets and go on with the mechanism
	current_state = idle;
        // notify the application that the data exchange was not successfull
	if(u->result) u->result(SOFA_ERROR);
	powercycle_turn_radio_off();
	PRINTF("sofamac: failed to parse (%u)\n", packetbuf_totlen ());
	return;
	}
    }

/*---------------------------------------------------------------------------*/
void sofamac_init(void)
    {
    PT_INIT(&pt);
    current_state = idle;
    CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
    PRINTDEBUG("Sofamac INIT\n");
    }

void sofamac_tx(uint16_t tx_value)
    {
    sofa_value = tx_value;
    if (current_state == idle)
   	{
   #if USE_BACKOFF
   	current_state = wait_to_send;
   	powercycle_turn_radio_on();
   #else
   	powercycle_turn_radio_on();
   	send_packet();
   #endif
	}
else{
//notify that we are not in idle mode
}
    }

/*---------------------------------------------------------------------------*/
static int turn_on(void)
    {
    if (current_state == disabled)
	{
	current_state = enabled;
	CSCHEDULE_POWERCYCLE(DEFAULT_OFF_TIME);
	}
    PRINTDEBUG("Sofamac ON\n");
    return 1;
    }

/*---------------------------------------------------------------------------*/
static int turn_off(int keep_radio_on)
    {
    current_state = disabled;
    PRINTDEBUG("Sofamac OFF\n");
    return NETSTACK_RADIO.off();
    }

static void qsend_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
    {

    }

void sofa_register(struct sofa_callback *f){
u = f;
}

/*---------------------------------------------------------------------------*/
/** Returns the channel check interval, expressed in clock_time_t ticks. */
static unsigned short channel_check_interval(void)
    {
    /*NOTE: DEFAULT_PERIOD=(RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE) CLOCK_SECOND */
    return (1ul * CLOCK_SECOND * DEFAULT_PERIOD) / RTIMER_ARCH_SECOND;
    }

/*---------------------------------------------------------------------------*/
const struct rdc_driver sofamac_driver =
  {
    "sofamac",
    sofamac_init,
    qsend_packet,
    qsend_list,
    input_packet,
    turn_on,
    turn_off,
    channel_check_interval,
  };

