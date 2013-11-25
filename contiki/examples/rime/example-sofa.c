#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime.h"

#include "lib/print-stats.h"

#include <stdio.h>
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif



/*
 *
 * Experiments options
 *
 * */
#define D_EST 1
#define ENABLE_RESPONSE_TIME 0
#define MSG_RATE 1

/*
 *
 * Global variables
 *
 * */

static struct unicast_conn unicast;
/*  Timers */
static struct etimer et;
#if D_EST
static struct ctimer send_timer;
#endif
/* Structures */
struct unicast_message {
    uint8_t type;
};
enum {
    UNICAST_TYPE_PING,
    UNICAST_TYPE_PONG
};
/* Costants */
#define MAX_NEIGHBORS 16
#define NEIGHBOR_DISCOVERY_CHANNEL 20
#define UNICAST_CHANNEL 40

/*
 *
 * PROCESS DEFINITION
 *
 * */
PROCESS(random_test_process, "Random test process");
AUTOSTART_PROCESSES(&random_test_process);

/*
 *
 *  FUNCTION DEFINITIONS
 *
 *  */

static void send_data(){
    ctimer_set(&send_timer,CLOCK_SECOND*MSG_RATE,(void (*)(void *))send_data, NULL);
    etimer_set(&et,(random_rand() % (CLOCK_SECOND*MSG_RATE)));
    //etimer_set(&et,10);
}

static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
    {
    //  struct unicast_message *msg;
    //  /* Grab the pointer to the incoming data. */
    //  PRINTF("unicast received from %d,%d",from->u8[0], from->u8[1]);
    //  msg = packetbuf_dataptr();
    //  /* We have two message types, UNICAST_TYPE_PING and
    //     UNICAST_TYPE_PONG. If we receive a UNICAST_TYPE_PING message, we
    //     print out a message and return a UNICAST_TYPE_PONG. */
    //  if(msg->type == UNICAST_TYPE_PING) {
    //    PRINTF("unicast ping received from %d.%d\n",
    //           from->u8[0], from->u8[1]);
    //    msg->type = UNICAST_TYPE_PONG;
    //    packetbuf_copyfrom(msg, sizeof(struct unicast_message));
    //    /* Send it back to where it came from. */
    //    unicast_send(c, from);
    //  }
    //  else if(msg->type == UNICAST_TYPE_PONG)
    //  {
    //   PRINTF("unicast pong received from %d.%d\n",
    //           from->u8[0], from->u8[1]);
    //  }
    }

/*
 *
 * REGISTER CALLBACKS
 *
 * */
static const struct unicast_callbacks unicast_callbacks = {recv_uc};

/*
 *
 * DEFINE MAIN PROCESS ev: event number, data: event data
 *
 */

PROCESS_THREAD(random_test_process, ev, data)
    {
    PROCESS_BEGIN();
    struct unicast_message msg;
    unicast_open(&unicast, UNICAST_CHANNEL, &unicast_callbacks);

    while(1)
	{
	etimer_set(&et,(CLOCK_SECOND*(MSG_RATE-1))+ (CLOCK_SECOND/2) + (random_rand() % CLOCK_SECOND) );
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
//	if(rimeaddr_node_addr.u8[0]==1)
//	    {
	//msg.type = UNICAST_TYPE_PING;
	//packetbuf_copyfrom(&msg, sizeof(msg));
	//unicast_send(&unicast, &rimeaddr_node_addr);
	sofamac_tx(5);
//	    }
	}

    /*
    ctimer_set(&send_timer,CLOCK_SECOND*MSG_RATE,(void (*)(void *))send_data, NULL);
    while(1){

	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	if(rimeaddr_node_addr.u8[0]==1)
	    {
	    msg.type = UNICAST_TYPE_PING;
	    packetbuf_copyfrom(&msg, sizeof(msg));
	    unicast_send(&unicast, &rimeaddr_node_addr);
	    }

    }
*/
    PROCESS_END();
    }
