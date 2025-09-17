#include "contiki.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "netstack.h"
#include "routing.h"
#include "uip-ds6.h"
#include "simple-udp.h"
#include "sys/log.h"
#include "anthocnet-pheromone.h"
#include "energest.h"
#include "../../../../../contiki-ng/os/sys/clock.h"
#define LOG_MODULE "AntHocNetProject"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_PORT 555
#define ARRAY_SIZE_ANTHOCPROJ 8


struct message {
    clock_time_t send_time;
    unsigned char random_data[ARRAY_SIZE_ANTHOCPROJ];
};

typedef struct
{
    struct message msg;
    uip_ipaddr_t sender;
} packet_id_t;

#define MAX_SEEN_PACKETS 64
static packet_id_t seen_packets[MAX_SEEN_PACKETS];
static int seen_packet_index = 0;

// check for duplicate
bool is_duplicate(packet_id_t *packet) {
    for (int i = 0; i < seen_packet_index; i++) {
        if (uip_ipaddr_cmp(&seen_packets[i].sender, &packet->sender) &&
            memcmp(&seen_packets[i].msg, &packet->msg, sizeof(struct message)) == 0) {
            return true;
            }
    }
    return false;
}

// add to seen packets
void add_seen_packet(packet_id_t *packet)
{
    seen_packets[seen_packet_index] = *packet;
    seen_packet_index = (seen_packet_index + 1) % MAX_SEEN_PACKETS;
}

static double
to_seconds(uint64_t time) {
    return (double)time / (double)ENERGEST_SECOND;
}

// upd callback function
static void
udp_rx_callback(struct simple_udp_connection *c, const uip_ipaddr_t *sender_addr, uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr, uint16_t receiver_port, const uint8_t *data, uint16_t datalen) {

    struct message *msg = (struct message *)data;
    clock_time_t time_difference = clock_time() - msg->send_time;

    packet_id_t packet = {.msg = *msg, .sender = *sender_addr};

    if (is_duplicate(&packet))
    {
        LOG_INFO("Duplicate packet received!\n");
        return;
    }
    add_seen_packet(&packet);

    LOG_INFO("UDP Package received from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_(" with length %d at %lu\n", datalen, clock_time());
    LOG_INFO("My time is: %lu\n", clock_time());
    LOG_INFO("Time difference was: %lu, that are %f seconds.\n", time_difference, (double)time_difference/CLOCK_SECOND);
    LOG_INFO("Payload was: ");
    for (int i = 0; i < ARRAY_SIZE_ANTHOCPROJ; i++) {
        LOG_INFO_("%d ", ((struct message *)data)->random_data[i]);
    }
    LOG_INFO_("\n");

}

static void
create_destination_address(uip_ipaddr_t *addr, uip_ipaddr_t *host_addr) {

    do
    {
        int id = rand() % 100 + 1; // random id between 1 and 100
        int i = id + 0x200;
        uip_ip6addr(addr, 0x2001, 0xdb8, 0x0, 0x0, i, id, id, id);
        LOG_INFO("Create destination address: ");
        LOG_INFO_6ADDR(addr);
        LOG_INFO_("\n");
    } while (uip_ipaddr_cmp(addr, host_addr));
}


PROCESS(anthocnettest, "AntHocNet multiple sender to one static destination run");
AUTOSTART_PROCESSES(&anthocnettest);

PROCESS_THREAD(anthocnettest, ev, data)
{
    static struct etimer start_timer;
    static struct etimer wait_timer;
    static struct etimer send_timer;
    static struct etimer end_timer;
    static struct etimer energest_timer;
    static uip_ipaddr_t host_addr;
    static uip_ipaddr_t destination_addr;
    PROCESS_BEGIN();

    host_addr = uip_ds6_get_global(ADDR_PREFERRED)->ipaddr;
    LOG_INFO("Host address: ");
    LOG_INFO_6ADDR(&host_addr);
    LOG_INFO_("\n");

    create_destination_address(&destination_addr, &host_addr);

    LOG_INFO("Host time is: %lu\n", clock_time());
    // set up udp
    static struct simple_udp_connection udp_conn;

    // set mote with mote id 1 as coordinator, create upd connection
    simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);

    int random_value = rand() % (108 * CLOCK_SECOND); // random value 0 and 108 seconds
    LOG_INFO("Random value for start timer: %d\n", random_value);

    etimer_set(&start_timer, random_value);
    // wait until all nodes are set up
    int random_value_wait = rand() % (10 * CLOCK_SECOND);
    etimer_set(&wait_timer, 120 * CLOCK_SECOND + random_value_wait);
    etimer_set(&end_timer, (2 * 60 * 60 + 108) * CLOCK_SECOND); // 2 hours
    etimer_set(&energest_timer, 120 * CLOCK_SECOND); // every 2 minutes

    while (1)
    {
        PROCESS_WAIT_EVENT();
        // wait until the node is associated. the init will start the broadcasting of hello messages.
        if (etimer_expired(&start_timer))
        {
            NETSTACK_ROUTING.init();
        }

        if (etimer_expired(&wait_timer))
        {
            etimer_set(&send_timer, 10 * CLOCK_SECOND);
            break;
        }
    }
    while (1) {
        PROCESS_WAIT_EVENT();
        if (etimer_expired(&send_timer)) {
            if (!uip_ipaddr_cmp(&host_addr, &destination_addr))
            {
                double send = (double)rand();
                // send with a probability of 10%
                if (send < RAND_MAX * 0.1) {
                    for (int i = 0; i < 5; i++)
                    {
                        unsigned char payload[ARRAY_SIZE_ANTHOCPROJ] = { 0, 4, 8, 16, 32, 64, 128, 255 };
                        struct message msg = {0};
                        msg.send_time = clock_time();
                        memcpy(msg.random_data, payload, ARRAY_SIZE_ANTHOCPROJ);
                        LOG_INFO("Send package with content: %lu", msg.send_time);
                        for (int j = 0; j < ARRAY_SIZE_ANTHOCPROJ; j++) {
                            LOG_INFO_(", %d", payload[j]);
                        }
                        LOG_INFO_("\n");
                        simple_udp_sendto_port(&udp_conn, &msg, sizeof(msg), &destination_addr, UDP_PORT);
                    }
                }
            }
            etimer_reset(&send_timer);
        }

        if (etimer_expired(&energest_timer)) {
            energest_flush();
            LOG_INFO("Energest\n");
            LOG_INFO("- CPU: %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_CPU)));
            LOG_INFO("- LPM: %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_LPM)));
            LOG_INFO("- DEEP LPM %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)));
            LOG_INFO("- Total time %f s\n", to_seconds(ENERGEST_GET_TOTAL_TIME()));
            LOG_INFO("- Radio LISTEN %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)));
            LOG_INFO("- Radio TRANSMIT %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)));
            LOG_INFO("- Radio OFF %f s\n", to_seconds(ENERGEST_GET_TOTAL_TIME()) - to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)) - to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)));
            etimer_reset(&energest_timer);
        }

        if (etimer_expired(&end_timer))
        {
            LOG_INFO("---------------Simulation-End---------------\n");
            LOG_INFO("Host address: ");
            LOG_INFO_6ADDR(&host_addr);
            LOG_INFO_("\n");

            energest_flush();
            LOG_INFO("Energest\n");
            LOG_INFO("- CPU: %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_CPU)));
            LOG_INFO("- LPM: %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_LPM)));
            LOG_INFO("- DEEP LPM %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)));
            LOG_INFO("- Total time %f s\n", to_seconds(ENERGEST_GET_TOTAL_TIME()));
            LOG_INFO("- Radio LISTEN %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)));
            LOG_INFO("- Radio TRANSMIT %f s\n", to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)));
            LOG_INFO("- Radio OFF %f s\n", to_seconds(ENERGEST_GET_TOTAL_TIME()) - to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)) - to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)));

            //LOG_INFO("Pheromone Table\n");
            //print_pheromone_table();
            LOG_INFO("End timer expired, stopping process.\n");
            NETSTACK_ROUTING.leave_network();
            PROCESS_EXIT();
        }
    }
    PROCESS_END();
}