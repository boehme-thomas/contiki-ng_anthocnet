#include "contiki.h"
#include "tcpip.h"
#include <stdio.h>
#include <anthocnet-pheromone.h>

#include "uip-ds6.h"
#include "uiplib.h"
#include "uip.h"

PROCESS(anthocnetpheromonetest, "Test for AntHocNet-Pheromone.h");
AUTOSTART_PROCESSES(&anthocnetpheromonetest);

PROCESS_THREAD(anthocnetpheromonetest, ev, data)
{
    static struct etimer start_timer;
    PROCESS_BEGIN();

    // wait ten seconds before anything happens
    etimer_set(&start_timer, 10 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&start_timer));

    printf("pheromone_table_init\n");
    // initialise the pheromone table
    pheromone_table_init();


    // set the imaginary neighbour destination that will be searched
    int accepted_neighbour_size = 0;
    uip_ipaddr_t neighbour_address;
    uip_ip6addr(&neighbour_address, 1, 2, 3, 4, 5, 6, 7, 8);
    printf("ip_address of neighbour ");
    uiplib_ipaddr_print(&neighbour_address);
    printf("\n");

    printf("\nget_neighbours_to_send_to_destination, should be NULL, forward ant true\n");

    // get the neighbour over which the imaginary neighbour should be reached
    // has to be null
    uip_ipaddr_t *neighbours = get_neighbours_to_send_to_destination(neighbour_address, true, &accepted_neighbour_size);

    // must be true
    if (neighbours == NULL) {
        printf("neighbours == NULL, size; %d\n", accepted_neighbour_size);
    }

    printf("\nget_neighbours_to_send_to_destination, should be NULL, forward ant false\n");

    neighbours = get_neighbours_to_send_to_destination(neighbour_address, false, &accepted_neighbour_size);
    if (neighbours == NULL) {
        printf("neighbours == NULL, size; %d\n", accepted_neighbour_size);
    }

    printf("\nget_pheromone_value, should be NULL\n");
    float *value = get_pheromone_value(neighbour_address, neighbour_address);
    if (value == NULL) {
        printf("Result was NULL\n");
    }

    uip_ipaddr_t host_addr;
    uip_ds6_addr_t *lladdr = uip_ds6_get_link_local(ADDR_PREFERRED);
    if (lladdr != NULL) {
        host_addr = lladdr->ipaddr;
    }

    // set imaginary path
    uip_ipaddr_t neighbour_1;
    uip_ip6addr(&neighbour_1, 1, 2, 3, 4, 5, 6, 7, 8);
    uip_ipaddr_t neighbour_2;
    uip_ip6addr(&neighbour_2, 2, 2, 2, 2, 2, 2, 2, 2);
    uip_ipaddr_t neighbour_3;
    uip_ip6addr(&neighbour_3, 3, 3, 3, 3, 3, 3, 3, 3);
    uip_ipaddr_t neighbour_4;
    uip_ip6addr(&neighbour_4, 4, 4, 4, 4, 4, 4, 4, 4);
    uip_ipaddr_t path[] = {neighbour_1, neighbour_2, neighbour_3, host_addr, neighbour_4};

    // create imaginary reactive backward ant
    struct reactive_backward_ant ant = {
    .ant_type = BACKWARD_ANT,
    .ant_generation = 2,
    .length = 5,
    .path = path,
    // when create_or_update_pheromone_table is called, then the current hop is already incremented
    .current_hop = 3,
    .time_estimate_T_P = (float)3.4
    };

    printf("\ncreate_or_update_pheromone_table\n");
    create_or_update_pheromone_table(ant);

    printf("\nget_pheromone_value, should not be NULL\n");
    float *value_n = get_pheromone_value(neighbour_2, neighbour_1);
    if (value_n == NULL) {
        printf("Result was NULL\n");
    } else {
        printf("Result was %f\n", *value_n);
    }

    // get newly created neighbour
    printf("\nget_neighbours_to_send_to_destination should not be NULL, forward ant false\n");
    neighbours = get_neighbours_to_send_to_destination(neighbour_1, false, &accepted_neighbour_size);
    // should get neighbour with 2s uip addr
    if (neighbours != NULL) {
        printf("neighbours != NULL, size; %d\n", accepted_neighbour_size);
        uiplib_ipaddr_print(&neighbours[0]);
    } else {
        printf("neighbours == NULL; \n");
    }

    // get the newly created pheromone value
    printf("\nget_pheromone_value, should not be NULL\n");
    value = get_pheromone_value(neighbour_2, neighbour_1);
    if (value == NULL) {
        printf("Result was NULL\n");
    } else {
        printf("Result was %f\n", *value);
    }

    // shoudnt do anything
    printf("\nAdd neighbour with known address, shouldn't do anything since neighbour should already exists\n");
    add_neighbour_to_pheromone_table(neighbour_2);

    // add new neighbour
    printf("\nAdd neighbour with unknown address, should work since its a new neighbour\n");
    uip_ipaddr_t new_neighbour;
    uip_ip6addr(&new_neighbour, 7, 7, 7, 7, 7, 7, 7, 7);
    add_neighbour_to_pheromone_table(new_neighbour);

    // delete unknow neighbour
    printf("\nDelete neighbour with unknown address, neighbour unknown, should do nothing\n");
    uip_ipaddr_t unknown_neighbour;
    uip_ip6addr(&unknown_neighbour, 8, 8, 8, 8, 8, 8, 8, 8);
    delete_neighbour_from_pheromone_table(unknown_neighbour);

    // delete known neighbour
    printf("\nDelete neighbour with known address, should delete neighbour\n");
    delete_neighbour_from_pheromone_table(new_neighbour);

    // delete destination entry from pheromone table with known addresses
    printf("\nDelete destination entry with known address, should delete destination\n");
    delete_destination_from_pheromone_table(neighbour_1, neighbour_2);


    printf("\n create_link_failure_notification_entries, should be NULL\n");
    int length;
    link_failure_notification_entry_t * lfn = creat_link_failure_notification_entries(neighbour_2, &length);
    if (lfn == NULL) {
        printf("link failure notification entry is NULL\n");
    } else {
        printf("link failure notification entry is not NULL\n");
        uiplib_ipaddr_print(&lfn->uip_address_of_destination);
    }


    printf("\n update_pheromone_after_link_failure\n");
    link_failure_notification_entry_t lfn_1 = {
        .uip_address_of_destination = neighbour_1,
        .number_of_hops_to_new_best_destination = 3,
        .time_estimate_T_P_of_new_best_destination = (float)6.2,
    };

    link_failure_notification_t lfn_2 = {
            .source = neighbour_2,
            .failed_link = new_neighbour,
            .entries = &lfn_1,
            .size_of_list_of_destinations = 1
    };
    update_pheromone_after_link_failure(lfn_2);

    PROCESS_END();
}
