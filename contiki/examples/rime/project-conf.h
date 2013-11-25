/* 
Contiki RDC drivers:
contikimac_driver
xmac_driver
cxmac_driver (more relaxed timing than the default X-MAC and therefore works on a broader set of radios)
lpp_driver
nullrdc_driver

Wiki:
http://www.sics.se/contiki/wiki/index.php/Change_MAC_or_Radio_Duty_Cycling_Protocols

*/

//change the RDC (radio duty cycle) channel check rate
//#undef NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
//#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 8

//specify what RDC driver Contiki should use
#undef NETSTACK_CONF_RDC
//#define NETSTACK_CONF_RDC nullrdc_driver
//#define NETSTACK_CONF_RDC sofalpl_driver
#define NETSTACK_CONF_RDC sofamac_driver

//#define NETSTACK_CONF_RDC sofalpp_driver
//#define NETSTACK_CONF_RDC cxmac_driver
//#define NETSTACK_CONF_RDC lpp_driver
//#define NETSTACK_CONF_RDC contikimac_driver


//specify what MAC driver Contiki should use
#undef NETSTACK_CONF_MAC
//#define NETSTACK_CONF_MAC csma_driver
#define NETSTACK_CONF_MAC nullmac_driver
