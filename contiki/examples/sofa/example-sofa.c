#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime.h"
#include "net/mac/sofamac.h"
#include "lib/print-stats.h"

#include <stdio.h>
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

// GLOBAL VARIABLES
//  Timer
static struct etimer et;
// Gossip Values
uint16_t received_value,node_value;
// Process definition
PROCESS(sofa_process, "Sofa example process");
AUTOSTART_PROCESSES(&sofa_process);

// FUNCTIONS DEFINITION

// This function is called when a message is received
static void sofa_rx(uint16_t rx_value){
PRINTF("Received value %d\n",rx_value);
received_value = rx_value;
}

// This function is called when a message exchange terminates
static void sofa_result(uint16_t ret_value){
switch ( ret_value ) {
case SOFA_SUCCESS:
  PRINTF("SofaApp: Successful message exchange\n");
  // since the data exchange was successful, we can aggregate the values (in this case, we average)
  node_value = (node_value + received_value)/2;
  printf("%d\n",node_value);
  break;
case SOFA_ERROR:
  // since the data exchange was not successful, we clear the recevied value
  received_value = 0;
  PRINTF("SofaApp: Message exchange failed\n");
  break;
case SOFA_BUSY:
  PRINTF("SofaApp: Message exchange rejected (busy channel)\n");
  break;
default:
  PRINTF("SofaApp: Unexpected return value from sofa's exchange!! \n");
  break;
}
}

// Get node's gossip value 
// This function can also be called by SOFA's pull mechanism
static uint16_t get_tx_value(){
return node_value;
}

// Register callbacks
struct sofa_callback sofa_callbacks = {sofa_rx, get_tx_value, sofa_result};

// Main process
PROCESS_THREAD(sofa_process, ev, data)
    {

    PROCESS_BEGIN();
    sofa_register(&sofa_callbacks);
    node_value = (rimeaddr_node_addr.u8[0])*10;
    received_value =0;
    while(1)
	{
	// wait for ~1.5 seconds
	etimer_set(&et,CLOCK_SECOND+(random_rand()%CLOCK_SECOND));
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	// try to send a packet
	sofamac_tx(get_tx_value());
	}
    PROCESS_END();
    }
