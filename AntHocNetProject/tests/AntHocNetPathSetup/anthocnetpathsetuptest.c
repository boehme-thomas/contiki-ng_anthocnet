//
// Created by thomas on 14.05.25.
//

#include "contiki.h"
#include "tcpip.h"
#include "anthocnet.h"
#include <stdio.h>
#include "anthocnet-types.h"
#include "anthocnet-conf.h"
#include "routing.h"
#include "uip-ds6.h"
#include "uiplib.h"

PROCESS(anthocnetpathsetuptest, "Test for AntHocNet-Path-Setup");
AUTOSTART_PROCESSES(&anthocnetpathsetuptest);

PROCESS_THREAD(anthocnetpathsetuptest, ev, data)
{
    NETSTACK_ROUTING.init();
    static struct etimer start_timer;
    PROCESS_BEGIN();

    // wait ten seconds before anything happens
    etimer_set(&start_timer, 10 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&start_timer));

    printf("update_running_average_T_i_mac\n");
    update_running_average_T_i_mac((float)0.4);


    uip_ipaddr_t neighbour_address;
    uip_ip6addr(&neighbour_address, 1, 2, 3, 4, 5, 6, 7, 8);

    uip_ipaddr_t destination_address;
    uip_ip6addr(&destination_address, 8, 7, 6, 5, 4, 3, 2, 1);

    uip_ipaddr_t host_addr;
    uip_ds6_addr_t *lladdr = uip_ds6_get_link_local(ADDR_PREFERRED);
    if (lladdr != NULL) {
        host_addr = lladdr->ipaddr;
    }

    uip_ipaddr_t neighbour_1;
    uip_ip6addr(&neighbour_1, 1, 1, 1, 1, 1, 1, 1, 1);
    uip_ipaddr_t neighbour_2;
    uip_ip6addr(&neighbour_2, 2, 2, 2, 2, 2, 2, 2, 2);
    uip_ipaddr_t neighbour_3;
    uip_ip6addr(&neighbour_3, 3, 3, 3, 3, 3, 3, 3, 3);
    uip_ipaddr_t neighbour_4;
    uip_ip6addr(&neighbour_4, 4, 4, 4, 4, 4, 4, 4, 4);
    uip_ipaddr_t path[] = {neighbour_1, neighbour_2, neighbour_3, neighbour_4};
    printf("uip_host addr: ");
    uiplib_ipaddr_print(&host_addr);
    printf("\n");


    struct reactive_forward_or_path_repair_ant ant =
    {
        .ant_type = REACTIVE_FORWARD_ANT,
        .ant_generation = 1,
        .source = neighbour_address,
        .destination = destination_address,
        .hops = 4,
        .path = path,
        .time_estimate_T_P = (float)4.0,
        .number_broadcasts = 0,
    };

    printf("\nsend_reactive_forward_or_path_repair_ant: false, next_hop, forward ant\n");
    send_reactive_forward_or_path_repair_ant(false, neighbour_address, ant);

    printf("\nsend_reactive_forward_or_path_repair_ant: true, next_hop, forward ant\n");
    uip_ipaddr_t uip_zeroes_addr;
    uip_ip6addr(&uip_zeroes_addr, 0, 0, 0, 0, 0, 0, 0, 0);
    send_reactive_forward_or_path_repair_ant(true, uip_zeroes_addr, ant);


    printf("\nreception reactive forward or path repair ant: ant, hops are not max, host is not destination");
    reception_reactive_forward_or_path_repair_ant(ant);


    struct reactive_forward_or_path_repair_ant ant_max_hops =
    {
        .ant_type = REACTIVE_FORWARD_ANT,
        .ant_generation = 1,
        .source = neighbour_address,
        .destination = destination_address,
        .hops = ANT_HOC_NET_MAX_HOPS,
        .path = path,
        .time_estimate_T_P = (float)4.0,
        .number_broadcasts = 0,
    };

    printf("\nreception reactive forward or path repair ant: ant, max hops reached, host is not destination");
    reception_reactive_forward_or_path_repair_ant(ant_max_hops);


    struct reactive_forward_or_path_repair_ant ant_host_is_dest =
    {
        .ant_type = REACTIVE_FORWARD_ANT,
        .ant_generation = 1,
        .source = neighbour_address,
        .destination = host_addr,
        .hops = 4,
        .path = path,
        .time_estimate_T_P = (float)4.0,
        .number_broadcasts = 0,
    };
    printf("\nreception reactive forward or path repair ant: ant, hops are not max, host is destination");
    reception_reactive_forward_or_path_repair_ant(ant_host_is_dest);


    uip_ipaddr_t neighbour_5;
    uip_ip6addr(&neighbour_5, 5, 5, 5, 5, 5, 5, 5, 5);
    uip_ipaddr_t neighbour_6;
    uip_ip6addr(&neighbour_6, 6, 6, 6, 6, 6, 6, 6, 6);
    uip_ipaddr_t neighbour_7;
    uip_ip6addr(&neighbour_7, 7, 7, 7, 7, 7, 7, 7, 7);
    uip_ipaddr_t path_new_gen[] = {neighbour_1, neighbour_5, neighbour_6, neighbour_7};

    struct reactive_forward_or_path_repair_ant ant_other_gen =
    {
        .ant_type = REACTIVE_FORWARD_ANT,
        .ant_generation = 2,
        .source = neighbour_address,
        .destination = destination_address,
        .hops = 4,
        .path = path_new_gen,
        .time_estimate_T_P = (float)1.0,
        .number_broadcasts = 0,
    };

    printf("\nreception reactive forward or path repair ant: current best exists, other generation, better time");
    reception_reactive_forward_or_path_repair_ant(ant_other_gen);


    uip_ipaddr_t path_better_time[] = {neighbour_1, neighbour_2, neighbour_3, neighbour_4};
    struct reactive_forward_or_path_repair_ant ant_better_time =
    {
        .ant_type = REACTIVE_FORWARD_ANT,
        .ant_generation = 1,
        .source = neighbour_address,
        .destination = destination_address,
        .hops = 4,
        .path = path_better_time,
        .time_estimate_T_P = (float)1.0,
        .number_broadcasts = 0,
    };
    printf("\nreception reactive forward or path repair ant: current best exists, same generation, better time");
    reception_reactive_forward_or_path_repair_ant(ant_better_time);


    uip_ipaddr_t path_different_first_hop[] = {neighbour_6, neighbour_2, neighbour_3, neighbour_4};
    struct reactive_forward_or_path_repair_ant ant_different_first_hop =
    {
        .ant_type = REACTIVE_FORWARD_ANT,
        .ant_generation = 1,
        .source = neighbour_address,
        .destination = destination_address,
        .hops = 4,
        .path = path_different_first_hop,
        .time_estimate_T_P = (float)10.0,
        .number_broadcasts = 0,
    };
    printf("\nreception reactive forward or path repair ant: current best exists, same generation, different first hop");
    reception_reactive_forward_or_path_repair_ant(ant_different_first_hop);

    uip_ipaddr_t neighbour_new_source;
    uip_ip6addr(&neighbour_new_source, 1, 3, 1, 3, 1, 3, 1, 3);
    struct reactive_forward_or_path_repair_ant ant_different_source =
    {
        .ant_type = REACTIVE_FORWARD_ANT,
        .ant_generation = 1,
        .source = neighbour_new_source,
        .destination = destination_address,
        .hops = 4,
        .path = path_different_first_hop,
        .time_estimate_T_P = (float)4.0,
        .number_broadcasts = 0,
    };
    printf("\nreception reactive forward or path repair ant: current best exists, different source");
    reception_reactive_forward_or_path_repair_ant(ant_different_source);

    printf("\ncreate and send backward ant: ant");
    create_and_send_backward_ant(ant.ant_generation, ant.hops, ant.path, ant.source);


    uip_ipaddr_t path2[] = {destination_address, neighbour_4, neighbour_3, host_addr, neighbour_1};
    struct reactive_backward_ant bw_ant = {
        .ant_type = BACKWARD_ANT,
        .ant_generation = 2,
        .destination = neighbour_6,
        .length = 5,
        .path = path2,
        .current_hop = 2,
        .time_estimate_T_P = (float)3.0,
    };

    printf("\nreception reactive backward ant: host not dest, current hop != length");
    reception_reactive_backward_ant(bw_ant);

    struct reactive_backward_ant bw_host_is_dest = {
        .ant_type = BACKWARD_ANT,
        .ant_generation = 2,
        .destination = host_addr,
        .length = 5,
        .path = path2,
        .current_hop = 2,
        .time_estimate_T_P = (float)3.0,
    };

    printf("\nreception reactive backward ant: destination is host");
    reception_reactive_backward_ant(bw_host_is_dest);

    uip_ipaddr_t path3[] = {destination_address, neighbour_4, neighbour_3, neighbour_1, host_addr};
    struct reactive_backward_ant bw_hop_is_len = {
        .ant_type = BACKWARD_ANT,
        .ant_generation = 2,
        .destination = neighbour_6,
        .length = 5,
        .path = path3,
        .current_hop = 3,
        .time_estimate_T_P = (float)3.0,
    };
    printf("\nreception reactive backward ant: host not dest, current hop == length (-1)");
    reception_reactive_backward_ant(bw_hop_is_len);

    printf("\nreactive path setup");
    reactive_path_setup(destination_address);

    printf("\n successfully done!");
    PROCESS_END();
}
