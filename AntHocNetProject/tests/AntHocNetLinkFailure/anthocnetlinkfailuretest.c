//
// Created by thomas on 14.05.25.
//

#include "contiki.h"
#include "tcpip.h"
#include "anthocnet.h"
#include <stdio.h>
#include <anthocnet-pheromone.h>
#include "anthocnet-types.h"
#include "routing.h"
#include "uip-ds6.h"
#include "uiplib.h"

PROCESS(anthocnetlinkfailuretest, "Test for AntHocNet-Link-Failure-Setup");
AUTOSTART_PROCESSES(&anthocnetlinkfailuretest);

PROCESS_THREAD(anthocnetlinkfailuretest, ev, data)
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

    uip_ipaddr_t neighbour_1;
    uip_ip6addr(&neighbour_1, 1, 1, 1, 1, 1, 1, 1, 1);

    uip_ipaddr_t neighbour_2;
    uip_ip6addr(&neighbour_2, 2, 2, 2, 2, 2, 2, 2, 2);

    /*
    uip_ipaddr_t host_addr;
    uip_ds6_addr_t *lladdr = uip_ds6_get_link_local(ADDR_PREFERRED);
    if (lladdr != NULL) {
        host_addr = lladdr->ipaddr;
    }
    */


    printf("\n data_transmission_to_neighbour_has_failed -> process start -> neighbour node has disssapeared -> broadcast link failure ");
    data_transmission_to_neighbour_has_failed(destination_address, neighbour_address);

    int length_of_notification_list;
    link_failure_notification_entry_t * notification_list = creat_link_failure_notification_entries(neighbour_address, &length_of_notification_list);
    link_failure_notification_t link_failure_notification = {
        .source = neighbour_1,
        .failed_link = neighbour_address,
        .size_of_list_of_destinations = length_of_notification_list,
        .entries = notification_list
    };

    printf("\n reception link failure notification");
    reception_link_failure_notification(link_failure_notification);

    printf("\n no pheromone value found while data transmission");
    no_pheromone_value_found_while_data_transmission(neighbour_2, destination_address);

    struct warning_message warning = {
        .packet_type = WARNING_MESSAGE,
        .destination = destination_address,
        .source =  neighbour_address
    };

    printf("\n reception warning");
    reception_warning(warning);


    printf("\n successfully done!");
    PROCESS_END();
}
