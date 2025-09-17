//
// Created by thomas on 14.05.25.
//

#include "contiki.h"
#include "anthocnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "anthocnet-types.h"
#include "anthocnet-conf.h"
#include "routing.h"
#include "uip-ds6.h"

PROCESS(anthocnetpathprobingtest, "Test for AntHocNet-Probing-Setup");
AUTOSTART_PROCESSES(&anthocnetpathprobingtest);

PROCESS_THREAD(anthocnetpathprobingtest, ev, data)
{
    NETSTACK_ROUTING.init();
    static struct etimer start_timer;
    PROCESS_BEGIN();

    // wait ten seconds before anything happens
    etimer_set(&start_timer, 10 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&start_timer));

    uip_ipaddr_t neighbour_address;
    uip_ip6addr(&neighbour_address, 1, 2, 3, 4, 5, 6, 7, 8);

    uip_ipaddr_t destination_address;
    uip_ip6addr(&destination_address, 8, 7, 6, 5, 4, 3, 2, 1);

    uip_ipaddr_t host_addr;
    uip_ds6_addr_t *lladdr = uip_ds6_get_link_local(ADDR_PREFERRED);
    if (lladdr != NULL) {
        host_addr = lladdr->ipaddr;
    }

    struct proactive_forward_ant ant = {
        .source = host_addr,
        .destination = destination_address,
        .number_of_broadcasts = 0,
        .hops = 0,
        .path = NULL
    };

    srand(time(NULL));
    printf("\nrand: %d, max: %d\n", rand(), RAND_MAX);
    printf("rand %d\n", rand());
    printf("rand %d\n", rand());
    printf("rand %d\n", rand());
    printf("rand %d\n", rand());
    printf("rand %d\n", rand());

    printf("\n broadcast hello message\n");
    broadcast_hello_messages();

    printf("\n send_proactive_forward_ant: number of broadcasts ok\n");
    send_proactive_forward_ant(ant);


    struct proactive_forward_ant ant_max_broadcasts = {
        .source = neighbour_address,
        .destination = destination_address,
        .number_of_broadcasts = ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PROACTIVE_FORWARD_A,
        .hops = 0,
        .path = NULL
    };
    printf("\n send_proactive_forward_ant: number of brodcasts at max\n");
    send_proactive_forward_ant(ant_max_broadcasts);

    uip_ipaddr_t neighbour_1;
    uip_ip6addr(&neighbour_1, 1, 1, 1, 1, 1, 1, 1, 1);
    uip_ipaddr_t neighbour_2;
    uip_ip6addr(&neighbour_2, 2, 2, 2, 2, 2, 2, 2, 2);
    uip_ipaddr_t path[] = {neighbour_1, neighbour_2};

    struct proactive_forward_ant ant_path = {
        .source = neighbour_address,
        .destination = destination_address,
        .number_of_broadcasts = ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PROACTIVE_FORWARD_A,
        .hops = 2,
        .path = path
    };
    printf("\n receive proactive forward ant: path not null\n");
    reception_proactive_forward_ant(ant_path);

    printf("\n receive proactive forward ant: path null\n");
    reception_proactive_forward_ant(ant_max_broadcasts);

    start_broadcast_of_hello_messages();
    broadcast_hello_messages();

    struct hello_message hello_message = {
        .source = neighbour_address
    };
    reception_hello_message(hello_message);

    printf("\n successfully done!");
    PROCESS_END();
}
