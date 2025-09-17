#include "contiki.h"
#include "tcpip.h"
#include <stdio.h>

#include "netstack.h"
#include "routing.h"
#include "tsch.h"
#include "uip-ds6.h"
#include "simple-udp.h"
#include "sys/log.h"
#define LOG_MODULE "AntHocNetProject"
#define LOG_LEVEL LOG_LEVEL_INFO

static linkaddr_t coordinator_addr =  {{ 0xf4, 0xce, 0x36, 0xda, 0xa6, 0xf8, 0x36, 0x2b }};
#define UDP_PORT 555
#define ARRAY_SIZE_ANTHOCPROJ 8

struct message {
    clock_time_t send_time;
    unsigned char random_data[ARRAY_SIZE_ANTHOCPROJ];
};

// upd callback function
static void
udp_rx_callback(struct simple_udp_connection *c, const uip_ipaddr_t *sender_addr, uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr, uint16_t receiver_port, const uint8_t *data, uint16_t datalen) {

    LOG_INFO("UDP Package received from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_(" with length %d at %lu\n", datalen, clock_time());
    clock_time_t time_difference = clock_time() - ((struct message *)data)->send_time;
    LOG_INFO("Time difference was: %lu, that are %f seconds.\n", time_difference, (double)time_difference/CLOCK_SECOND);
    LOG_INFO("Payload was: ");
    for (int i = 0; i < ARRAY_SIZE_ANTHOCPROJ; i++) {
        LOG_INFO_("%d ", ((struct message *)data)->random_data[i]);
    }
    LOG_INFO_("\n");
}


PROCESS(anthocnettest, "Test for AntHocNet");
AUTOSTART_PROCESSES(&anthocnettest);

PROCESS_THREAD(anthocnettest, ev, data)
{
    static struct etimer start_timer;
    static struct etimer send_timer;
    static uip_ipaddr_t host_addr;
    static uip_ipaddr_t cooja_addr;
    static uip_ipaddr_t destination_addr;

    uip_ip6addr(&cooja_addr, 0x2001, 0xdb8, 0x0, 0x0, 0x201, 0x1, 0x1, 0x1);
    uip_ip6addr(&destination_addr, 0x2001, 0xdb8, 0x0, 0x0, 0x204, 0x4, 0x4, 0x4);

    host_addr = uip_ds6_get_global(ADDR_PREFERRED)->ipaddr;
    LOG_INFO("Host address: ");
    LOG_INFO_6ADDR(&host_addr);
    LOG_INFO_("\n");

    // set up udp
    static struct simple_udp_connection udp_conn;

    // set mote with mote id 1 as coordinator, create upd connection
    if (uip_ipaddr_cmp(&host_addr, &cooja_addr) || linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
        tsch_set_coordinator(1);
        simple_udp_register(&udp_conn, UDP_PORT, &destination_addr, UDP_PORT, udp_rx_callback);
    } else if (uip_ipaddr_cmp(&host_addr, &destination_addr)) {
        simple_udp_register(&udp_conn, UDP_PORT, &cooja_addr, UDP_PORT, udp_rx_callback);
    }

    PROCESS_BEGIN();

    // wait 2 seconds for the mote to set up
    etimer_set(&start_timer, 2 * CLOCK_SECOND);

    while (1) {
        // wait until the node is associated. the init will start the broadcasting of hello messages.
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&start_timer));
        if (tsch_is_associated) {
            printf("Tsch is assosicated call NETSTACK.init\n");
            NETSTACK_ROUTING.init();
            etimer_stop(&start_timer);
            break;
        }
        etimer_reset(&start_timer);
    }

    if (uip_ipaddr_cmp(&host_addr, &cooja_addr) || linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
        etimer_set(&send_timer, 123 * CLOCK_SECOND);
        while (1) {
            // wait 30 seconds to send a package to cooja mote with address 4
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));
            unsigned char payload[ARRAY_SIZE_ANTHOCPROJ] = { 2, 4, 8, 16, 32, 64, 128, 255 };
            struct message msg = {0};
            msg.send_time = clock_time();
            memcpy(msg.random_data, payload, ARRAY_SIZE_ANTHOCPROJ);
            LOG_INFO("Send package with content: %lu", msg.send_time);
            for (int i = 0; i < ARRAY_SIZE_ANTHOCPROJ; i++) {
                LOG_INFO_(", %d", payload[i]);
            }
            LOG_INFO_("\n");
            simple_udp_send(&udp_conn, &msg, sizeof(msg));
            etimer_reset(&send_timer);
        }
    }
    PROCESS_END();
}
