#include "contiki.h"
#include "tcpip.h"
#include <stdio.h>

#include "netstack.h"
#include "routing.h"
#include "uip-ds6.h"
#include "simple-udp.h"
#include "sys/log.h"
#include "anthocnet-pheromone.h"
#include "energest.h"
#define LOG_MODULE "AntHocNetProject"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_PORT 555
#define ARRAY_SIZE_ANTHOCPROJ 8

struct message {
    clock_time_t send_time;
    unsigned char random_data[ARRAY_SIZE_ANTHOCPROJ];
};

static unsigned long
to_seconds(uint64_t time) {
    return (unsigned long)(time / ENERGEST_SECOND);
}

// upd callback function
static void
udp_rx_callback(struct simple_udp_connection *c, const uip_ipaddr_t *sender_addr, uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr, uint16_t receiver_port, const uint8_t *data, uint16_t datalen) {

    LOG_INFO("UDP Package received from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_(" with length %d at %lu\n", datalen, clock_time());
    struct message *msg = (struct message *)data;
    clock_time_t time_difference = clock_time() - msg->send_time;
    LOG_INFO("My time is: %lu\n", clock_time());
    LOG_INFO("Time difference was: %lu, that are %f seconds.\n", time_difference, (double)time_difference/CLOCK_SECOND);
    LOG_INFO("Payload was: ");
    for (int i = 0; i < ARRAY_SIZE_ANTHOCPROJ; i++) {
        LOG_INFO_("%d ", ((struct message *)data)->random_data[i]);
    }
    LOG_INFO_("\n");

}


PROCESS(anthocnettest, "AntHocNet burst every two minutes run");
AUTOSTART_PROCESSES(&anthocnettest);

PROCESS_THREAD(anthocnettest, ev, data)
{
    static struct etimer start_timer;
    static struct etimer send_timer;
    static struct etimer end_timer;
    static uip_ipaddr_t host_addr;
    static uip_ipaddr_t cooja_addr;
    static uip_ipaddr_t destination_addr;

    PROCESS_BEGIN();

    uip_ip6addr(&cooja_addr, 0x2001, 0xdb8, 0x0, 0x0, 0x201, 0x1, 0x1, 0x1);
    uip_ip6addr(&destination_addr, 0x2001, 0xdb8, 0x0, 0x0, 0x204, 0x4, 0x4, 0x4);

    host_addr = uip_ds6_get_global(ADDR_PREFERRED)->ipaddr;
    LOG_INFO("Host address: ");
    LOG_INFO_6ADDR(&host_addr);
    LOG_INFO_("\n");

    LOG_INFO("Host time is: %lu\n", clock_time());
    // set up udp
    static struct simple_udp_connection udp_conn;

    // set mote with mote id 1 as coordinator, create upd connection
    if (uip_ipaddr_cmp(&host_addr, &cooja_addr)) {
        simple_udp_register(&udp_conn, UDP_PORT, &destination_addr, UDP_PORT, udp_rx_callback);
    } else if (uip_ipaddr_cmp(&host_addr, &destination_addr)) {
        simple_udp_register(&udp_conn, UDP_PORT, &cooja_addr, UDP_PORT, udp_rx_callback);
    }

    // wait 2 seconds for the mote to set up
    etimer_set(&start_timer, 2 * CLOCK_SECOND);
    etimer_set(&end_timer, (11 * 60 + 30) * CLOCK_SECOND); // 11:30 minutes

    while (1) {
        // wait until the node is associated. the init will start the broadcasting of hello messages.
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&start_timer));
        NETSTACK_ROUTING.init();
        break;
    }

    if (uip_ipaddr_cmp(&host_addr, &cooja_addr)) {
        etimer_set(&send_timer, 123 * CLOCK_SECOND);
        while (1) {
            PROCESS_WAIT_EVENT();
            if (etimer_expired(&end_timer)) {
                LOG_INFO("---------------Simulation-End---------------\n");
                LOG_INFO("Host address: ");
                LOG_INFO_6ADDR(&host_addr);
                LOG_INFO_("\n");

                energest_flush();
                LOG_INFO("Energest\n");
                LOG_INFO("- CPU: %4lus\n", to_seconds(energest_type_time(ENERGEST_TYPE_CPU)));
                LOG_INFO("- LPM: %4lus\n", to_seconds(energest_type_time(ENERGEST_TYPE_LPM)));
                LOG_INFO("- DEEP LPM %4lus\n", to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)));
                LOG_INFO("- Total time %lus\n", to_seconds(ENERGEST_GET_TOTAL_TIME()));
                LOG_INFO("- Radio LISTEN %lus\n", to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)));
                LOG_INFO("- Radio TRANSMIT %lus\n", to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)));
                LOG_INFO("- Radio OFF %lus\n", to_seconds(ENERGEST_GET_TOTAL_TIME()) - to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)) - to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)));

                LOG_INFO("Pheromone Table\n");
                print_pheromone_table();
                LOG_INFO("End timer expired, stopping process.\n");
                PROCESS_EXIT();
            }
            if (etimer_expired(&send_timer)) {
                for (int i = 0; i < 5; i++) {
                    unsigned char payload[ARRAY_SIZE_ANTHOCPROJ] = { 0, 4, 8, 16, 32, 64, 128, 255 };
                    struct message msg = {0};
                    msg.send_time = clock_time();
                    memcpy(&payload[0], &i, sizeof(unsigned char));
                    memcpy(msg.random_data, payload, ARRAY_SIZE_ANTHOCPROJ);
                    LOG_INFO("Send package with content: %lu", msg.send_time);
                    for (int j = 0; j < ARRAY_SIZE_ANTHOCPROJ; j++) {
                        LOG_INFO_(", %d", payload[j]);
                    }
                    LOG_INFO_("\n");
                    simple_udp_send(&udp_conn, &msg, sizeof(msg));
                }
                etimer_reset(&send_timer);
            }
        }
    }
    PROCESS_END();
}
