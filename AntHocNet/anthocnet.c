/**
 * \file
 *      Implementation of the AntHocNet routing protocol, developed by Di Carlo, Ducatelle and Gambardella.\n
 *      The paper can be found here (https://onlinelibrary.wiley.com/doi/10.1002/ett.1062).
 */

#include "anthocnet.h"
#include "anthocnet-pheromone.h"
#include "anthocnet-conf.h"
#include "../../contiki-ng/os/net/routing/routing.h"

#if MAC_CONF_WITH_TSCH
// include tsch.h since the compiler throws an error with tsch-queue.h
#include "tsch.h"
#elif MAC_CONF_WITH_CSMA
#include "csma-output.h"
#endif

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "anthocnet-icmpv6.h"
#include "uip-ds6.h"
#include "uip-icmp6.h"
#include "tcpip.h"

// logging
#include "sys/log.h"
#define LOG_MODULE "AntHocNet"
#ifdef LOG_CONF_LEVEL_ANTHOCNET_MAIN
#define LOG_LEVEL LOG_CONF_LEVEL_ANTHOCNET_MAIN
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif


void calc_time_estimate_T_P(float* time_estimate_T_P);
void create_reactive_forward_or_path_repair_ant(unsigned int ant_gen, uip_ipaddr_t destination, packet_type_t type_of_ant);
void broadcast_link_failure_notification(link_failure_notification_t link_failure_notification);
void delete_neighbour_from_best_ant_array(uip_ipaddr_t neighbour_address);
void create_and_send_proactive_forward_ant(uip_ipaddr_t destination);
void delete_last_destination_data_array();
void discard_buffer();
void send_multicast_message(int icmp_type, uip_ipaddr_t next_hop, int len);

static bool initialized = false;
static bool hello_message_broadcasting = false;
static bool acceptance_messages = false;
static float running_average_T_i_mac;
static unsigned int ant_generation;
static best_ants_t *best_ants;
static uip_ipaddr_t host_addr;
static uip_ipaddr_t uip_zeroes_addr;
static last_package_data_t last_package_data;
static last_destination_data_t *last_destination_data;
static buffer_t buffer;
static uip_ipaddr_t multicast_addr;

/*----Start-Processes-------------------------------------------------------------------------------------------------*/
PROCESS(broadcast_hello_messages_proc, "Broadcast Hello Messages Process");
PROCESS(reactive_path_setup_proc, "Reactive Path Setup Process");
PROCESS(data_transmission_failed_proc, "Data Transmission Failed");

/*
 * Process that handles the reactive path setup.
 * Sends reactive forward ant, and waits till a backward ant arrives.
 * If the ant doesn't arrive in ANT_HOC_NET_RESTART_PATH_SETUP_SECS seconds, another ant is sent.
 * The process repeats the path setup, if not successful, ANT_HOC_NET_MAX_TRIES_PATH_SETUP times.
 */
PROCESS_THREAD(reactive_path_setup_proc, ev, data) {
    static int try_counter;
    static unsigned int ant_gen;
    static struct etimer timer;
    static uip_ipaddr_t destination;

    PROCESS_BEGIN();
    try_counter = 0;
    ant_gen = 0;

    packet_buffer_t *new_packet = malloc(sizeof(packet_buffer_t));
    if (!new_packet) {
        LOG_ERR("Failed to allocate new packet");
        PROCESS_EXIT();
    }
    if (uip_len <= 0)
    {
        LOG_ERR("uIP len was 0, cannot buffer empty packet");
        free(new_packet);
        PROCESS_EXIT();
    }
    new_packet->buffer = malloc(sizeof(unsigned char) * uip_len);
    if (!new_packet->buffer) {
        LOG_ERR("Failed to allocate buffer for new packet");
        free(new_packet);
        PROCESS_EXIT();
    }
    new_packet->len = uip_len;
    new_packet->next = NULL;
    // put the packet at the end of the buffer
    packet_buffer_t *current_packet = buffer.packet_buffer;
    if (current_packet == NULL) {
        buffer.packet_buffer = new_packet;
    } else {
        while (current_packet->next != NULL)
        {
            current_packet = current_packet->next;
        }
        current_packet->next = new_packet;
    }

    memcpy(new_packet->buffer, &uip_buf, new_packet->len);

    ++buffer.number_of_packets;
    buffer.valid = true;
    memcpy(&destination, data, sizeof(uip_ipaddr_t));

    LOG_INFO("Reactive path setup process started\n");

    LOG_DBG("Create ant with destination ");
    LOG_DBG_6ADDR(&destination);
    LOG_DBG_("\n");

    try_counter++;
    // remember ant generation for the backward ant
    ant_gen = ++ant_generation;
    // send reactive forward ant
    LOG_DBG("Sending reactive forward ant\n");
    create_reactive_forward_or_path_repair_ant(ant_gen, destination, REACTIVE_FORWARD_ANT);

    // set timer
    etimer_set(&timer, ANT_HOC_NET_RESTART_PATH_SETUP_SECS * CLOCK_SECOND);

    do {
        // wait until an event is received
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        try_counter++;

        // save ant gen for backward ant
        ant_gen = ++ant_generation;

        // send reactive forward ant
        LOG_DBG("No ant came back, Sending reactive forward ant\n");
        create_reactive_forward_or_path_repair_ant(ant_gen, destination, REACTIVE_FORWARD_ANT);
        LOG_DBG("Ant was sent; destination address is: ");
        LOG_DBG_6ADDR(&destination);
        LOG_DBG_("\n");

        // set timer
        etimer_reset(&timer);
        LOG_DBG("Destination address is: ");
        LOG_DBG_6ADDR(&destination);
        LOG_DBG_("\n");

    } while (try_counter <= ANT_HOC_NET_MAX_TRIES_PATH_SETUP);
    // no backward ant was received -> discard saved package
    LOG_DBG("No backward ant was received -> discard saved package\n");
    discard_buffer();

    PROCESS_END();
}

/*
 * Process that send hello messages every ANT_HOC_NET_T_HELLO_SEC seconds.
 */
PROCESS_THREAD(broadcast_hello_messages_proc, ev, data) {
    PROCESS_BEGIN();
    LOG_INFO("Hello messages broadcasting process started\n");

    static struct etimer timer;

    // set timer
    etimer_set(&timer, ANT_HOC_NET_T_HELLO_SEC * CLOCK_SECOND);

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        broadcast_hello_messages();
        etimer_reset(&timer);
    }
    PROCESS_END();
    LOG_INFO("Hello messages broadcasting process ended\n");
}

/*
 * Process that handles the failure while a data transmission
 */
PROCESS_THREAD(data_transmission_failed_proc, ev, data) {
    static struct etimer timer;

    static uip_ipaddr_t destination;
    static uip_ipaddr_t neighbour;

    PROCESS_BEGIN();

    memcpy(&destination, data, sizeof(uip_ipaddr_t));
    memcpy(&neighbour, data + sizeof(uip_ipaddr_t), sizeof(uip_ipaddr_t));

    float *estimated_time = get_pheromone_value(neighbour, destination);

    // to be safe, that should not happen, since the neighbour is not yet deleted
    if (estimated_time == NULL) {
        PROCESS_EXIT();
    }

    // calculate seconds to wait, according to the paper
    unsigned long seconds = (unsigned long) (CLOCK_SECOND * ANT_HOC_NET_FACTOR_OF_WAITING_TIME_BRA * (*estimated_time));

    // broadcast path repair ant like a reactive forward ant
    ++ant_generation;
    create_reactive_forward_or_path_repair_ant(ant_generation, destination, PATH_REPAIR_ANT);

    etimer_set(&timer, seconds);

    while (1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
            discard_buffer();
            // if no BRA ant is received in that time, send a link failure notification
            neighbour_node_has_disappeared(neighbour);
            PROCESS_EXIT();
    }

    PROCESS_END();
}

/*----End-Processes---------------------------------------------------------------------------------------------------*/

/*----General-Functions-----------------------------------------------------------------------------------------------*/

int accept_messages() {
    return acceptance_messages;
}

int processes_running() {
    return process_is_running(&reactive_path_setup_proc) || process_is_running(&data_transmission_failed_proc);
}

void stop_reactive_path_setup_and_data_transmission_failed_process() {
    process_exit(&reactive_path_setup_proc);
    process_exit(&data_transmission_failed_proc);
}

void send_buffered_data_packages() {
    LOG_INFO("Backward ant is at its destination! Send buffered packages!\n");
    if (buffer.valid)
    {
        int buffer_len = buffer.number_of_packets;
        for (int i = 0; i < buffer_len; ++i) {
            if (buffer.packet_buffer->buffer != NULL) {
                if (buffer.packet_buffer->buffer != NULL && buffer.packet_buffer->len > 0) {
                    LOG_INFO("Send message of length %d\n", buffer.packet_buffer->len);
                    uip_len = buffer.packet_buffer->len;
                    memcpy(&uip_buf, buffer.packet_buffer->buffer, buffer.packet_buffer->len);
                    tcpip_ipv6_output();
                }

                --buffer.number_of_packets;
                packet_buffer_t *temp = buffer.packet_buffer;
                buffer.packet_buffer = buffer.packet_buffer->next;
                if (temp != NULL) {
                    if (temp->buffer != NULL) {
                        free(temp->buffer);
                        temp->buffer = NULL;
                    }
                    if (temp != NULL) {
                        free(temp);
                    }
                }
            }
        }
        if (buffer.number_of_packets == 0) {
            buffer.packet_buffer = NULL;
            buffer.valid = false;
        }
        // if the length is not 0 then new packages were put into the queue
        // thus the buffer is not invalid
    }
}

/**
 * Discards the buffered packets.
 */
void discard_buffer() {
    if (buffer.valid) {
        packet_buffer_t *packet_buffer = buffer.packet_buffer;
        while (packet_buffer != NULL) {
            packet_buffer_t *temp = packet_buffer;
            packet_buffer = packet_buffer->next;
            if (temp != NULL) {
                if (temp->buffer != NULL) {
                    free(temp->buffer);
                    temp->buffer = NULL;
                }
                free(temp);
                temp = NULL;
            }
        }
        buffer.packet_buffer = NULL;
        buffer.number_of_packets = 0;
        buffer.valid = false;
    }
    LOG_INFO("Buffer discarded!\n");
}

unsigned int get_current_ant_generation() {
    return ant_generation;
}

uip_ipaddr_t get_host_address() {
    return host_addr;
}
/*
void send_multicast_message(int icmp_type, uip_ipaddr_t next_hop, int len) {

    LOG_DBG("Sending multicast message with type %u\n", icmp_type);
    LOG_DBG("len: %u and ", len);
    LOG_DBG_6ADDR(&next_hop);
    LOG_DBG_("\n");

    UIP_IP_BUF->vtc = 0x60;
    UIP_IP_BUF->tcflow = 0;
    UIP_IP_BUF->flow = 0;
    UIP_IP_BUF->proto = UIP_PROTO_ICMP6;
    UIP_IP_BUF->ttl = uip_ds6_if.cur_hop_limit;
    uipbuf_set_len_field(UIP_IP_BUF, UIP_ICMPH_LEN + len);

    memcpy(&UIP_IP_BUF->destipaddr, &next_hop, sizeof(uip_ipaddr_t));
    uip_ds6_select_src(&UIP_IP_BUF->srcipaddr, &UIP_IP_BUF->destipaddr);
    LOG_DBG("src ip: ");
    LOG_DBG_6ADDR(&UIP_IP_BUF->srcipaddr);
    LOG_DBG_("\n");

    UIP_ICMP_BUF->type = icmp_type;
    UIP_ICMP_BUF->icode = 0;

    UIP_ICMP_BUF->icmpchksum = 0;
    UIP_ICMP_BUF->icmpchksum = ~uip_icmp6chksum();

    uip_len = UIP_IPH_LEN + UIP_ICMPH_LEN + len;

    LOG_DBG("uip len: %d\n", uip_len);

    UIP_STAT(++uip_stat.icmp.sent);
    UIP_STAT(++uip_stat.ip.sent);

    LOG_INFO("Sending ICMPv6 packet to ");
    LOG_INFO_6ADDR(&UIP_IP_BUF->destipaddr);
    LOG_INFO_(", type %d, code %d\n", 0, icmp_type);

    LOG_DBG("uip_len: %d (UIP_IPH_LEN + UIP_ICMPH_LEN + len)\n", uip_len);
    LOG_DBG("uip_buf len field %u; [0]: %d, [1] %d\n", uipbuf_get_len_field(UIP_IP_BUF), UIP_IP_BUF->len[0], UIP_IP_BUF->len[1]);
    LOG_DBG("(uip_len - UIP_IPH_LEN) >> 8): %d\n", (uip_len - UIP_IPH_LEN) >> 8);
    LOG_DBG("(uip_len - UIP_IPH_LEN) & 0xff): %d\n", ((uip_len - UIP_IPH_LEN) & 0xff));
    //UIP_IP_BUF->len[0] = ((uip_len - UIP_IPH_LEN) >> 8);
    //UIP_IP_BUF->len[1] = ((uip_len - UIP_IPH_LEN) & 0xff);
    //LOG_DBG("uip_buf len field %u; [0]: %d, [1] %d\n", uipbuf_get_len_field(UIP_IP_BUF), UIP_IP_BUF->len[0], UIP_IP_BUF->len[1]);

    UIP_MCAST6.out();
}
*/
/*----End-General-Functions-------------------------------------------------------------------------------------------*/

/*----Reactive-Path-Setup---------------------------------------------------------------------------------------------*/

/**
 * Calculates time_estimate_T_P (^T_script_P), the estimate of the time it would take to travel over path P to the destination d.
 * It is used to update routing tables. For that it calculates the estimate of each node to reach the next hop.\n
 * This function corresponds to the equations (3) and (2) of the AntHocNet paper.
 * @param time_estimate_T_P The time estimate of the ant, which is going to be updated
 */
void calc_time_estimate_T_P(float* time_estimate_T_P) {

    if (time_estimate_T_P == NULL) {
        LOG_ERR("Time estimate is NULL!\n");
        return;
    }
    int Q_i_mac = 0;

#if MAC_CONF_WITH_TSCH
    // get all packages in all TSCH queues and assigned them to the Q_i_mac
    Q_i_mac = tsch_queue_global_packet_count();
#elif MAC_CONF_WITH_CSMA
    Q_i_mac = get_packet_count();
#else
#error Only CSMA or TSCH are supported, whereas CSMA should be selected for cooja simulations / when the minimal tsch is used
#endif

    /* running average is updated every time a packet is sent
        float time_t_i_mac = (float)0.0;
        update_running_average_T_i_mac(time_t_i_mac);
    */

    // equation (3) in AntHocNet paper; product of the average time to send one packet running_average_T_i_mac and the current
    // number of packets in the queue
    float product_of_avg_mac_time = (float)(Q_i_mac + 1) * running_average_T_i_mac;

    // equation (2) in AntHocNet paper
    *time_estimate_T_P += product_of_avg_mac_time;
}

void update_running_average_T_i_mac(float new_time_t_i_mac) {
    // equation (4)
    running_average_T_i_mac = ANT_HOC_NET_ALPHA * running_average_T_i_mac + (1 - ANT_HOC_NET_ALPHA) * (new_time_t_i_mac/ (float)CLOCK_SECOND);
    LOG_DBG("Running average T_i_mac updated to: %f\n", running_average_T_i_mac);
}

void send_reactive_forward_or_path_repair_ant(bool broadcast, uip_ipaddr_t next_hop, struct reactive_forward_or_path_repair_ant ant) {

    // check if the address is valid
    if (!broadcast && uip_ipaddr_cmp(&next_hop, &uip_zeroes_addr)) {
        // the next hop address was not valid
        return;
    }

    if (broadcast) {
        // broadcast ant; since no broadcast is available in IPv6, select multicast address
        uip_create_linklocal_allnodes_mcast(&next_hop);
        //uip_ipaddr_copy(&next_hop, &multicast_addr);
    }

    char *ant_type_str = (ant.ant_type == REACTIVE_FORWARD_ANT) ? "Reactive Forward Ant" : "Path Repair Ant";
    LOG_INFO("%s sent with destination: ", ant_type_str);
    LOG_INFO_6ADDR(&ant.destination);
    char *broadcast_str = broadcast ? "broadcast" : "unicast";
    LOG_INFO_(" as %s\n", broadcast_str);

    // copy the first 7 fields
    uint16_t size_counter = sizeof(ant) - sizeof(uip_ipaddr_t *);
    memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN], &ant, size_counter);

    // copy the whole path
    if (ant.hops >= 1  && ant.path != NULL) {
        memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN + size_counter], ant.path, ant.hops * sizeof(uip_ipaddr_t));
        size_counter += ant.hops * sizeof(uip_ipaddr_t);
    }
    /*if (broadcast) {
        send_multicast_message(ICMP6_REACTIVE_FORWARD_ANT, next_hop, size_counter);
    } else {*/
        uip_icmp6_send(&next_hop, ICMP6_REACTIVE_FORWARD_ANT, ant.ant_type, size_counter);
   // }
}

/**
 * Creates reactive forward ant or path repair ant and calls send_reactive_forward_or_path_repair_ant to broadcast ant.
 * @param ant_gen The generation of the new ant
 * @param destination The destination the ant is searching
 * @param type_of_ant The type of the ant. Should be either REACTIVE_FORWARD_ANT or PATH_REPAIR_ANT
 */
void create_reactive_forward_or_path_repair_ant(unsigned int ant_gen, uip_ipaddr_t destination, packet_type_t type_of_ant) {

    // type has to be reactive forward ant or path repair ant
    if (type_of_ant != REACTIVE_FORWARD_ANT && type_of_ant != PATH_REPAIR_ANT) {
        // add logging?
        return;
    }

    struct reactive_forward_or_path_repair_ant ant;
    ant.ant_generation = ant_gen;
    ant.source = host_addr;
    memcpy(&ant.destination, &destination, sizeof(uip_ipaddr_t));
    // The source node is not the first hop
    ant.hops = 0;
    // The source node is not in the path
    ant.path = NULL;
    // Source time estimate is not part of the time estimate of the ant
    ant.time_estimate_T_P = (float)0.0;
    // Set the ant type
    ant.ant_type = type_of_ant;
    // Set the number of broadcasts
    ant.number_broadcasts = 0;

    send_reactive_forward_or_path_repair_ant(true, uip_zeroes_addr, ant);
}

void reception_reactive_forward_or_path_repair_ant(struct reactive_forward_or_path_repair_ant ant) {
    LOG_DBG("Reactive forward ant or path repair ant received!\n");


    if (uip_ipaddr_cmp(&ant.source, &host_addr)) {
        LOG_DBG("Host is source node! - Ignore ant!\n");
        // ignore ant when node is the source and gets it back
        if (ant.path != NULL) {
            free(ant.path);
            ant.path = NULL;
        }
        return;
    }

    for (int i = 0; i < ant.hops; ++i) {
        LOG_DBG("ant.path[%d]: ", i);
        LOG_DBG_6ADDR(&ant.path[i]);
        LOG_DBG_("\n");
        if (uip_ipaddr_cmp(&host_addr, &ant.path[i])) {
            LOG_DBG("Host is in path -> ant already visited this node -> probably received from a broadcast!\n");
            if (ant.path != NULL) {
                free(ant.path);
                ant.path = NULL;
            }
            return;
        }
    }

    ant.hops++;

    // check if the maximum of the hops is reached if so discard ant before computing it further
    if (ant.hops > ANT_HOC_NET_MAX_HOPS) {
        LOG_DBG("Max hops reached!\n");
        if (ant.path != NULL) {
            free(ant.path);
            ant.path = NULL;
        }
        return;
    }

    // add node to the path (even if it's the destination)
    uip_ipaddr_t *new_path = malloc(ant.hops * sizeof(uip_ipaddr_t));

    if (ant.path != NULL) {
        // copy old elements, thus - 1
        memcpy(new_path, ant.path, (ant.hops - 1) * sizeof(uip_ipaddr_t));
        new_path[ant.hops - 1] = host_addr;
        free(ant.path);
        ant.path = new_path;
    } else {
        // path is null, thus path[0] can be the host addr
        ant.path = new_path;
        ant.path[0] = host_addr;
    }

    // Node is destination, then send backward ant
    if (uip_ipaddr_cmp(&ant.destination, &host_addr)) {
        LOG_DBG("Destination reached!\n");
        LOG_INFO("Length of path of forward ant at destination: %d\n", ant.hops);
        create_and_send_backward_ant(ant.ant_generation, ant.hops, ant.path, ant.source);
        return;
    }


    // Calc update time estimate
    calc_time_estimate_T_P(&ant.time_estimate_T_P);

    // Acceptation of ant.
    // The ant is accepted if its estimated time is in the range of the best ant with the acceptance factor a1;
    // if the ant is not accepted with a1, its first hop it looked up in the first hops array, if not in that array,
    // ant is accepted if its estimated time is in the range of the best ant with the acceptance factor a2.
    // Atm the decision is made without respect to the hop count.

    best_ants_t *current_best_ants = best_ants;
    best_ants_t *last_best_ants = NULL;

    // select best_ants_t enty where the address is the same as from the source of the ant
    while (current_best_ants != NULL) {
        if (uip_ipaddr_cmp(&current_best_ants->source, &ant.source)) {
            break;
        }
        last_best_ants = current_best_ants;
        current_best_ants = current_best_ants->next;
    }

    if (current_best_ants == NULL) {
        LOG_DBG("No best ant exists or no entry where the addresses are the same are found\n");
        // no best ant exists or no entry where the addresses are the same are found
        // create a new best_ant
        best_ants_t *new_best_ants = (best_ants_t*) malloc(sizeof(best_ants_t));
        new_best_ants->source = ant.source;
        new_best_ants->best_ants_per_generation_array = malloc(sizeof(best_ant_t));
        new_best_ants->next = NULL;

        // create new best ant array
        new_best_ants->best_ants_per_generation_array->generation = ant.ant_generation;
        new_best_ants->best_ants_per_generation_array->hop_count = ant.hops;
        new_best_ants->best_ants_per_generation_array->time_estimate = ant.time_estimate_T_P;

        // ant path is never NULL, initial path is set up above
        new_best_ants->best_ants_per_generation_array->first_hops = malloc(sizeof(uip_ipaddr_t));
        new_best_ants->best_ants_per_generation_array->first_hops[0] = ant.path[0];
        new_best_ants->best_ants_per_generation_array->first_hops_len = 1;
        new_best_ants->size_of_best_ants_per_generation_array = 1;

        // if no best_ants were yet found
        if (best_ants == NULL) {
            // set new best ant as the new global best ants
            best_ants = new_best_ants;
        } else if (last_best_ants != NULL) {
            // add new best ants to the existing array; that happens when a new source was detected
            last_best_ants->next = new_best_ants;
        }
        LOG_DBG("Set new best ant!\n");
    } else {
        LOG_DBG("Best ant exists!\n");
        // reached if a best_ants_t entry with the same address as the address of the source of the ant was found

        unsigned int number = current_best_ants->size_of_best_ants_per_generation_array;

        // if the ant found a new best path
        bool best = false;

        // if an ant with the same generation has already been seen
        bool seen = false;

        for (int i = 0; i < number; ++i) {
            // if current generation in the loop is the generation of the ant
            if (current_best_ants->best_ants_per_generation_array[i].generation == ant.ant_generation) {
                seen = true;
                LOG_DBG("An best ant with the same generation exists!\n");
                float est_of_best_entry = current_best_ants->best_ants_per_generation_array[i].time_estimate;

                // check acceptance factor a1

                // max, so that ACC_FACTOR_A1 can be greater or smaller than 1
                float threshold_a1 = est_of_best_entry * (float)fmax(ANT_HOC_NET_ACC_FACTOR_A1, 1.0 / ANT_HOC_NET_ACC_FACTOR_A1);

                LOG_DBG("Estimated time of best ant: %f\n", est_of_best_entry);
                LOG_DBG("Threshold for acceptance factor a1: %f\n", threshold_a1);
                LOG_DBG("Estimated time of ant: %f\n", ant.time_estimate_T_P);
                if (ant.time_estimate_T_P <= threshold_a1) {
                    LOG_DBG("Ant time estimate is <= threshold.\n");
                    // if time estimate of the ant is smaller than that of the current best ant of the generation
                    if (ant.time_estimate_T_P < est_of_best_entry) {
                        LOG_DBG("Ant is new best Ant.\n");
                        best = true;
                    }

                    // failed to get accepted with acceptance factor A1
                    // check if first hop of ant is different from those of the previously accepted ants
                } else {
                    LOG_DBG("Ant time estimate is > threshold - ant failed to get accepted with threshold 1.\n");
                    // check if first hop of ant exists in the first hop list
                    for (int j = 0; j < current_best_ants->best_ants_per_generation_array[i].first_hops_len; j++) {

                        /* if path was empty before reception on this ant, path[0] is now this node -> no need to check
                         * whether path[0] is valid
                         */
                        // if the first hop of the ant is a hop, that was used before, the ant is not accepted
                        if (uip_ipaddr_cmp(&current_best_ants->best_ants_per_generation_array[i].first_hops[j], &ant.path[0])) {
                            LOG_DBG("Ant doesn't have unique first path - ant is not accepted!\n");
                            // ant is not accepted
                            if (ant.path != NULL) {
                                free(ant.path);
                                ant.path = NULL;
                            }
                            return;
                        }
                    }

                    LOG_DBG("Ant has unique first hop! -> check acceptance with threshold 2\n");

                    // if first hop of ant does not exist in the first hop list, check for acceptance factor a2
                    float threshold_a2 = est_of_best_entry * (float)fmax(ANT_HOC_NET_ACC_FACTOR_A2, 1.0 / ANT_HOC_NET_ACC_FACTOR_A2);
                    LOG_DBG("Estimated time of best ant: %f\n", est_of_best_entry);
                    LOG_DBG("Threshold for acceptance factor a2: %f\n", threshold_a2);
                    LOG_DBG("Estimated time of ant: %f\n", ant.time_estimate_T_P);
                    if (ant.time_estimate_T_P > threshold_a2) {
                        LOG_DBG("Ant time estimate is > threshold2 - ant failed to get accepted with threshold 2.\n");
                        LOG_DBG("Ant is killed.\n");
                        // ant is not accepted
                        if (ant.path != NULL) {
                            free(ant.path);
                            ant.path = NULL;
                        }
                        return;
                    }
                }
                LOG_DBG("Ant is accepted!\n");
                //------reached when ant was accepted-------------------------------------------------------------------

                current_best_ants->best_ants_per_generation_array[i].first_hops_len++;

                // add first hop of the node to the first hops array
                uip_ipaddr_t *new_first_hops = malloc(current_best_ants->best_ants_per_generation_array[i].first_hops_len * sizeof(uip_ipaddr_t));
                memcpy(new_first_hops, current_best_ants->best_ants_per_generation_array[i].first_hops, (current_best_ants->best_ants_per_generation_array[i].first_hops_len - 1) * sizeof(uip_ipaddr_t));

                new_first_hops[current_best_ants->best_ants_per_generation_array[i].first_hops_len - 1] = ant.path[0];
                if (current_best_ants->best_ants_per_generation_array[i].first_hops != NULL) {
                    free(current_best_ants->best_ants_per_generation_array[i].first_hops);
                    current_best_ants->best_ants_per_generation_array[i].first_hops = NULL;
                }
                current_best_ants->best_ants_per_generation_array[i].first_hops = new_first_hops;

                // if best, set values of ant to the new best ant
                if (best) {
                    LOG_DBG("Update best ant array!\n");
                    current_best_ants->best_ants_per_generation_array[i].hop_count = ant.hops;
                    current_best_ants->best_ants_per_generation_array[i].time_estimate = ant.time_estimate_T_P;
                }

                break;
            }
        }
        /* THIS SECTION IS ONLY REACHED IF THE ANT WAS ACCEPTED OR NO ANT OF ITS GENERATION WAS EVER SEEN,
         * THUS IF AN ANT WAS NOT ACCEPTED, THIS AREA WOULD NOT BE REACHED
         */

        // check whether the loop ended from a break of because no best ant was found
        if (!seen) {
            LOG_DBG("No ant of the same generation was seen before!\n");
            // if this section is reached, if no ant of that ant.generation was received earlier
            // therefor ant is accepted and added to best_ants.best_ants_per_generation_array

            current_best_ants->size_of_best_ants_per_generation_array++;

            best_ant_t new_best_ant;
            new_best_ant.time_estimate = ant.time_estimate_T_P;
            new_best_ant.generation = ant.ant_generation;
            new_best_ant.hop_count = ant.hops;
            new_best_ant.first_hops = malloc(sizeof(uip_ipaddr_t));
            new_best_ant.first_hops[0] = uip_zeroes_addr;
            if (ant.hops > 0) {
                new_best_ant.first_hops[0] = ant.path[0];
                new_best_ant.first_hops_len = 1;
            } else {
                new_best_ant.first_hops_len = 0;
            }

            // add new best_ant element to the array
            best_ant_t *new_best_ant_array = malloc(current_best_ants->size_of_best_ants_per_generation_array * sizeof(best_ant_t));
            memcpy(new_best_ant_array, current_best_ants->best_ants_per_generation_array,
                   (current_best_ants->size_of_best_ants_per_generation_array - 1) * sizeof(best_ant_t));
            memcpy(&new_best_ant_array[current_best_ants->size_of_best_ants_per_generation_array - 1], &new_best_ant, sizeof(new_best_ant));
            if (current_best_ants->best_ants_per_generation_array != NULL) {
                free(current_best_ants->best_ants_per_generation_array);
                current_best_ants->best_ants_per_generation_array = NULL;
            }
            current_best_ants->best_ants_per_generation_array = new_best_ant_array;
        }
    }

    // Check for routing information, if existent, then unicast if not broadcast
    // Select next neighbours

    int size_of_neighbours = 0;
    uip_ipaddr_t *list_of_neighbours_with_destination = get_neighbours_to_send_to_destination(ant.destination, true, &size_of_neighbours);

    // Multicast if no pheromone value is no available
    // Select neighbour to unicast to with probability Pnd
    if (size_of_neighbours == 0) {
        LOG_DBG("No neighbours to unicast to!\n");
        ant.number_broadcasts++;
        // for path repair ant, check whether the number of broadcasts is below the allowed number
        if (!(ant.ant_type == PATH_REPAIR_ANT && ant.number_broadcasts >= ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PATH_REPAIR_A)) {
            send_reactive_forward_or_path_repair_ant(true, uip_zeroes_addr, ant);
        }

    } else {
        /*
        for (int i = 0; i < size_of_neighbours; i++) {
            send_reactive_forward_or_path_repair_ant(false, list_of_neighbours_with_destination[i], ant);
        }
        */
        // only sent it to one found neighbour
        send_reactive_forward_or_path_repair_ant(false, list_of_neighbours_with_destination[0], ant);
    }
    if (list_of_neighbours_with_destination != NULL) {
        free(list_of_neighbours_with_destination);
        list_of_neighbours_with_destination = NULL;
    }

    if (ant.path != NULL) {
        // free the allocated space for the path of the ant
        free(ant.path);
        ant.path = NULL;
    }
}

void create_and_send_backward_ant(unsigned int ant_gen, hop_t hops, uip_ipaddr_t * path, uip_ipaddr_t destination) {
    LOG_DBG("Start create and send backward ant!\n");
    uip_ipaddr_t reversed_path[hops];

    if (path == NULL) {
        LOG_DBG("Path is NULL! - no backward ant is sent!\n");
        return;
    }

    // reverse path
    for (int i = 0; i < hops; i++) {
        reversed_path[i] = path[(hops - 1) - i];
    }

    struct reactive_backward_ant rba = {
            .ant_type = BACKWARD_ANT,
            .ant_generation = ant_gen,
            .destination = destination,
            .path = reversed_path,
            .length = hops,
            .time_estimate_T_P = (float)0.0,
            .current_hop = 0,
    };

    // check whether the next neighbour is still there, if not discard the ant
    if (!does_neighbour_exists(reversed_path[1])) {
        LOG_DBG("Neighbour ");
        LOG_DBG_6ADDR(&reversed_path[1]);
        LOG_DBG_(" is not reachable, since it doesn't exist anymore!\n");
        if (path != NULL) {
            free(path);
            path = NULL;
        }
        return;
    }

    LOG_INFO("Backward ant sent with destination: ");
    LOG_INFO_6ADDR(&rba.destination);
    LOG_INFO_("\n");

    // copy the first fields
    uint16_t size_counter = sizeof(rba) - sizeof(uip_ipaddr_t *);
    memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN], &rba, size_counter);

    // copy the whole path
    if (rba.length >= 1  && rba.path != NULL) {
        memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN + size_counter], rba.path, rba.length * sizeof(uip_ipaddr_t));
        size_counter += rba.length * sizeof(uip_ipaddr_t);
    }
    uip_icmp6_send(&reversed_path[1], ICMP6_REACTIVE_BACKWARD_ANT, 0, size_counter);

    // free the allocated space for the path of the forward ant
    if (path != NULL) {
        free(path);
        path = NULL;
    }
}

void reception_reactive_backward_ant(struct reactive_backward_ant ant) {
    LOG_DBG("Start reception reactive backward ant!\n");

    ++ant.current_hop;
    LOG_DBG("Current hop is %d\n", ant.current_hop);
    LOG_DBG("Length of path is %d\n", ant.length);

    calc_time_estimate_T_P(&ant.time_estimate_T_P);
    create_or_update_pheromone_table(ant);

    if (uip_ipaddr_cmp(&ant.destination, &host_addr)) {
        LOG_DBG("Destination reached!\n");
        if (ant.path != NULL) {
            free(ant.path);
            ant.path = NULL;
        }
        return;
    }

    uip_ipaddr_t next_neighbour_addr;

    // length minus one since the first node set the current_hop to 0;
    // length minus one since it is the length of the path with the source node and the current hop doesnt contain it
    if (ant.length - 1 == ant.current_hop) {
        // backward ant reached the last hop before the destination,
        // so send the ant to the destination
        LOG_DBG("Last hop before the destination reached!\n");
        next_neighbour_addr = ant.destination;
    } else {
        next_neighbour_addr = ant.path[ant.current_hop + 1];
    }
    LOG_DBG("Next hop neighbour is ");
    LOG_DBG_6ADDR(&next_neighbour_addr);
    LOG_DBG_("\n");

    // check whether the next neighbour is still there, if not discard the ant
    if (!does_neighbour_exists(next_neighbour_addr)) {
        LOG_DBG("Next hop neighbour is not reachable!\n");
        if (ant.path != NULL) {
            free(ant.path);
            ant.path = NULL;
        }
        return;
    }

    LOG_DBG("Send backward ant to next neighbour!\n");
    LOG_DBG("Path: \n");
    for (int i = 0; i < ant.length; i++) {
        LOG_DBG("[%d]: ", i);
        LOG_DBG_6ADDR(&ant.path[i]);
        LOG_DBG_("\n");
    }

    // copy the first fields
    uint16_t size_counter = sizeof(ant) - sizeof(uip_ipaddr_t *);
    memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN], &ant, size_counter);

    // copy the whole path
    if (ant.length >= 1  && ant.path != NULL) {
        memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN + size_counter], ant.path, ant.length * sizeof(uip_ipaddr_t));
        size_counter += ant.length * sizeof(uip_ipaddr_t);
    }

    LOG_INFO("Backward Ant sent to neighbour: ");
    LOG_INFO_6ADDR(&next_neighbour_addr);
    LOG_INFO_("\n");
    uip_icmp6_send(&next_neighbour_addr, ICMP6_REACTIVE_BACKWARD_ANT, 0, size_counter);
}

void reactive_path_setup(uip_ipaddr_t destination) {
    process_start(&reactive_path_setup_proc, (process_data_t *) &destination);
}

void delete_neighbour_from_best_ant_array(uip_ipaddr_t neighbour_address) {
    best_ants_t * current = best_ants;
    best_ants_t * previous = NULL;
    while (current != NULL) {
        if (uip_ipaddr_cmp(&current->source, &neighbour_address)) {
            if (previous == NULL) {
                best_ants = current->next;
            } else {
                previous->next = current->next;
            }
            if (current->best_ants_per_generation_array->first_hops != NULL) {
                free(current->best_ants_per_generation_array->first_hops);
                current->best_ants_per_generation_array->first_hops = NULL;
            }
            if (current->best_ants_per_generation_array != NULL) {
                free(current->best_ants_per_generation_array);
                current->best_ants_per_generation_array = NULL;
            }
            if (current != NULL) {
                free(current);
                current = NULL;
            }
            return;
        }
        previous = current;
        current = current->next;
    }
}

/**
 * Deletes the best ants array.
 */
void delete_best_ants_array () {
    while (best_ants != NULL) {
        if (best_ants->best_ants_per_generation_array->first_hops != NULL) {
            free(best_ants->best_ants_per_generation_array->first_hops);
            best_ants->best_ants_per_generation_array->first_hops = NULL;
        }
        if (best_ants->best_ants_per_generation_array != NULL) {
            free(best_ants->best_ants_per_generation_array);
            best_ants->best_ants_per_generation_array = NULL;
        }
        best_ants_t * temp = best_ants;
        best_ants = best_ants->next;
        free(temp);
        temp = NULL;
    }
    best_ants = NULL;
}

/*----End-Reactive-Path-Setup-----------------------------------------------------------------------------------------*/

/*----Stochastic-data-routing-----------------------------------------------------------------------------------------*/

int stochastic_data_routing(uip_ipaddr_t destination, uip_ipaddr_t *address) {
    LOG_DBG("Start stochastic_data_routing\n");
    LOG_DBG("Send data to neighbour with address: ");
    LOG_DBG_6ADDR(&destination);
    LOG_DBG_("\n");
    int size_of_accepted_neighbours;

    // only one neighbour is selected at the time being, but the list contains all
    uip_ipaddr_t *accepted_neighbours = get_neighbours_to_send_to_destination(destination, false, &size_of_accepted_neighbours);

    // if a neighbour is found, return 1 and the address
    if (accepted_neighbours != NULL && size_of_accepted_neighbours > 0) {
        LOG_DBG("Stochastic data routing: Neighbour found\n");

        memcpy(address, &accepted_neighbours[0], sizeof(uip_ipaddr_t));
        LOG_DBG("Data copied!\n");

        // safe the last package
        last_package_data.destination = destination;
        last_package_data.selected_nexthop = accepted_neighbours[0];
        last_package_data.len = uip_len;
        if (last_package_data.buffer != NULL) {
            free(last_package_data.buffer);
            last_package_data.buffer = NULL;
        }
        last_package_data.buffer = malloc(uip_len);
        memcpy(last_package_data.buffer, &uip_buf, uip_len);

        // check for path probing only if we are the source node and not a forwarding node
        if (uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &host_addr)) {
            LOG_DBG("Check for path probing!\n");
            last_destination_data_t *dest_data = last_destination_data;

            // check if packages were sent to that destination in a specific time frame, if so check counter
            // if the sending rate is reached, sent proactive forward ants
            clock_time_t now = clock_time();
            bool seen = false;
            last_destination_data_t *found_dest = NULL;
            last_destination_data_t *accepted_dest = NULL;
            while (dest_data != NULL) {
                double seconds_dif = (double)(now - dest_data->time) / (double)CLOCK_SECOND;
                LOG_DBG("Dest time difference: %f!\n", seconds_dif);
                // if time diff is greater than 0.0028 seconds remove the destination
                // if messages are sent to the same destination in a short time, it is assumed that a data session is running
                // if that's the case, count the messages and send proactive forward ant according to the sending rate.
                LOG_DBG("Dest data: count: %d, time: %lu, addr: ", dest_data->count, dest_data->time);
                LOG_DBG_6ADDR(&dest_data->destination);
                LOG_DBG_("\n");

                if (seconds_dif <= ANT_HOC_NET_PFA_TIME_THRESHOLD) {
                    LOG_DBG("Dest time difference is smaller than 0.5 seconds!\n");
                    LOG_DBG("Safe the destination!\n");

                    // safe the destination data if it's in the time frame and the correct destination
                    if (uip_ipaddr_cmp(&dest_data->destination, &destination)) {
                        LOG_DBG("Destination is found!\n");
                        found_dest = dest_data;
                        seen = true;
                    }

                    // safe dest as accepted
                    if (accepted_dest == NULL) {
                        // if no other destination was found, set dest as the only accepted
                        accepted_dest = dest_data;
                    } else {
                        // if another destination was found before, safe it as the next element
                        accepted_dest->next = dest_data;
                    }

                    // move to the next element
                    dest_data = dest_data->next;

                } else {
                    LOG_DBG("Dest time difference is greater than 0.5 seconds!\n");
                    LOG_DBG("Remove dest data, even if it's not the one with the right destination!\n");

                    // free the current dest data and continue to the next element
                    last_destination_data_t *temp = dest_data;
                    dest_data = dest_data->next;
                    free(temp);
                    temp = NULL;
                }
            }

            // safe the accepted destinations as the last destination data
            last_destination_data = accepted_dest;

            if (seen && found_dest != NULL) {
                LOG_DBG("Seen and found!\n");
                found_dest->count++;
                found_dest->time = now;
                if (found_dest->count == ANT_HOC_NET_PFA_SENDING_RATE_N) {
                    LOG_DBG("Path Probing: Sent Proactive forward ant\n");
                    create_and_send_proactive_forward_ant(destination);

                    // copy the data back since the data of the ant may overwrite the buffer
                    if (last_package_data.buffer != NULL) {
                        uip_len = last_package_data.len;
                        memcpy(&uip_buf, last_package_data.buffer, last_package_data.len);
                        free(last_package_data.buffer);
                        last_package_data.buffer = NULL;
                    }
                    found_dest->count = 0;
                }

            } else {
                LOG_DBG("No last dest found!\n");
                // if no last destination is found create a new one
                last_destination_data_t *new_dest = malloc(sizeof(last_destination_data_t));
                if (new_dest != NULL) {
                    new_dest->destination = destination;
                    new_dest->time = now;
                    new_dest->count = 1;
                    new_dest->next = last_destination_data;
                    last_destination_data = new_dest;
                    LOG_DBG("New destination data set.\n");
                }
            }

        }
        if (accepted_neighbours != NULL) {
            free(accepted_neighbours);
            accepted_neighbours = NULL;
        }
        LOG_DBG("Found address: ");
        LOG_DBG_6ADDR(address);
        LOG_DBG_("\n");
        return 1;
    }
    LOG_DBG("No neighbour was found!\n");
    // if no neighbour was found

    // free the allocated neighbour list
    if (accepted_neighbours != NULL) {
        free(accepted_neighbours);
        accepted_neighbours = NULL;
    }

    // this function is also called when packages were incoming. so check if the source address of the package is this
    // node's uip address, if not, and no neighbours were found, a "dangling" link, as it was called in the paper,
    // was taken
    if (!uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &host_addr)) {
        LOG_DBG("Stochastic data routing: No neighbour found while data transmission \"dangling link\" taken\n");
        no_pheromone_value_found_while_data_transmission(UIP_IP_BUF->srcipaddr, destination);
        return 0;
    }

    // if processes are running and new package is found where the host is the source, buffer that message.
    if (uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &host_addr) && processes_running()) {
        LOG_INFO("Packet buffered for later sending, since the reactive path setup or data transmission failed processes is running!\n");
        packet_buffer_t *next_packet = malloc(sizeof(packet_buffer_t));
        next_packet->buffer = malloc(uip_len);
        memcpy(next_packet->buffer, &uip_buf, uip_len);
        next_packet->len = uip_len;
        next_packet->next = NULL;

        // append package at the end of the buffer to send the packages in the right order
        if (buffer.packet_buffer == NULL) {
            buffer.packet_buffer = next_packet;
        } else {
            packet_buffer_t *current = buffer.packet_buffer;
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = next_packet;
        }
        ++buffer.number_of_packets;

        buffer.valid = true;
        return 0;
    }

    LOG_DBG("Stochastic data routing: Start path setup\n");
    // start the reactive path setup phase
    reactive_path_setup(destination);

    // when 0 is returned, get_nexthop will check if routes are available for that destination. in the configuration
    // file the route list was configured to the size of 0, so the function will find none and will return NULL.
    // after that it will check if default routes are available. in the configuration that size is also set to 0, so
    // that function will also find none and return NULL.
    // get_nexthop will then therefore return NULL too, which will result in tcpip_ipv6_output deleting
    // the uip buffer and exiting.
    // that means that the package can be sent "again" from the reactive path setup process after an backward ant is
    // received.
    return 0;
}

void delete_last_destination_data_array() {
    while (last_destination_data != NULL) {
        last_destination_data_t *temp = last_destination_data;
        last_destination_data = last_destination_data->next;
        free(temp);
    }
}

/*----End-Stochastic-data-routing-------------------------------------------------------------------------------------*/

/*----Proactive-path-probing,-maintenance-and-exploration-------------------------------------------------------------*/

/**
 * Creates a proactive forward ant and sends it.
 * @param destination The final destination of the ant (in contrast to the next hop)
 */
void create_and_send_proactive_forward_ant(uip_ipaddr_t destination) {
    LOG_DBG("Send proactive forward ant\n");
    struct proactive_forward_ant ant = {
            .source = host_addr,
            .destination = destination,
            .number_of_broadcasts = 0,
            .hops = 0,
            .path = NULL
    };

    send_proactive_forward_ant(ant);
}

void send_proactive_forward_ant(struct proactive_forward_ant ant) {
    LOG_DBG("Send proactive forward ant\n");

    uip_ipaddr_t next_hop;
    srand(time(NULL));
    double random_number = (double) rand() / RAND_MAX;

    bool broadcast = false;

    // check whether the ant is broadcast
    if (random_number <= ANT_HOC_NET_PFA_BROADCAST_PROBABILITY) {
        broadcast = true;

    } else {
        // get next hop neighbour
        // at the time being, it is only one neighbour selected
        int accepted_neighbour_size = 0;
        uip_ipaddr_t *neighbours = get_neighbours_to_send_to_destination(ant.destination, true, &accepted_neighbour_size);

        if (accepted_neighbour_size == 0 || neighbours == NULL) {
            LOG_DBG("Send ant as broadcast!\n");
            broadcast = true;
        } else {
            LOG_DBG("Send ant as unicast to ");
            LOG_DBG_6ADDR(&neighbours[0]);
            LOG_DBG_("\n");
            // only one ant should be selected
            next_hop = neighbours[0];
        }
        if (neighbours != NULL) {
            free(neighbours);
            neighbours = NULL;
        }
    }
    if (broadcast) {
        // kill ant if the maximal number of broadcasts is reached
        if (ant.number_of_broadcasts == ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PFA) {
            LOG_DBG("Maximal number of broadcasts reached, kill ant!\n");
            if (ant.path != NULL) {
                free(ant.path);
                ant.path = NULL;
            }
            return;
        }
        uip_create_linklocal_allnodes_mcast(&next_hop);
        //uip_ipaddr_copy(&next_hop, &multicast_addr);
        ++ant.number_of_broadcasts;
    }

    // copy the first fields
    uint16_t size_counter = sizeof(ant) - sizeof(uip_ipaddr_t *);
    memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN], &ant, size_counter);

    // copy the whole path
    if (ant.hops >= 1  && ant.path != NULL) {
        memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN + size_counter], ant.path, ant.hops * sizeof(uip_ipaddr_t));
        size_counter += ant.hops * sizeof(uip_ipaddr_t);
    }

    /*if (broadcast) {
        send_multicast_message(ICMP6_PROACTIVE_FORWARD_ANT, next_hop, size_counter);
    } else {*/

    LOG_INFO("Proactive forward ant sent with destination: ");
    LOG_INFO_6ADDR(&ant.destination);
    char *broadcast_str = broadcast ? "broadcast" : "unicast";
    LOG_INFO_(" as %s\n", broadcast_str);

    uip_icmp6_send(&next_hop, ICMP6_PROACTIVE_FORWARD_ANT, 0, size_counter);
    //}

}

void reception_proactive_forward_ant(struct proactive_forward_ant ant) {
    LOG_DBG("Received proactive forward ant\n");
    ant.hops++;

    // add node to the path
    uip_ipaddr_t *new_path = malloc(ant.hops * sizeof(uip_ipaddr_t));
    memcpy(new_path, ant.path, (ant.hops - 1) * sizeof(uip_ipaddr_t));
    new_path[ant.hops - 1] = host_addr;
    ant.path = new_path;

    if (uip_ipaddr_cmp(&ant.destination, &host_addr)) {
        LOG_DBG("Destination reached!\n");
        create_and_send_backward_ant((int)ant_generation, ant.hops, ant.path, ant.source);
    } else {
        LOG_DBG("Send proactive forward ant to next neighbour!\n");
        send_proactive_forward_ant(ant);
    }
}

/**
 * Starts the process that broadcast hello messages every ANT_HOC_NET_T_HELLO_SECs.
 */
void start_broadcast_of_hello_messages() {
    hello_message_broadcasting = true;
    process_start(&broadcast_hello_messages_proc, (process_data_t *) NULL);
}

/**
 * Stops the process that broadcast hello messages.
 */
void stop_broadcast_of_hello_messages() {
    hello_message_broadcasting = false;
    process_exit(&broadcast_hello_messages_proc);
}

void broadcast_hello_messages() {

    uip_ipaddr_t next_hop;
    uip_create_linklocal_allnodes_mcast(&next_hop);
    //uip_ipaddr_copy(&next_hop, &multicast_addr);

    struct hello_message hello_msg;
    hello_msg.source = host_addr;
    hello_msg.time_estimate_T_P = (float)0.0;
    calc_time_estimate_T_P(&hello_msg.time_estimate_T_P);

    // if time estimate is 0, set it to 1.0, to have a valid value at the receiving nodes (and not 200)
    if (hello_msg.time_estimate_T_P == 0.0) {
        hello_msg.time_estimate_T_P = (float)1.0;
    }

    uint8_t* uip_buf_copy = NULL;
    uint16_t len = 0;
    if (uip_len > 0) {
        LOG_DBG("UIP interrupted by hello broadcast - Data copied!\n");
        uip_buf_copy = malloc(uip_len);
        memcpy(uip_buf_copy, &uip_buf, uip_len);
        len = uip_len;
    }

    LOG_DBG("Hello message broadcasted\n");

    memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN], &hello_msg, sizeof(struct hello_message));
    uip_icmp6_send(&next_hop, ICMP6_HELLO_MESSAGE, 0, sizeof(hello_msg));
    //send_multicast_message(ICMP6_HELLO_MESSAGE, next_hop, sizeof(struct hello_message));

    LOG_DBG("Done broadcasting hello message\n");
    if (uip_buf_copy != NULL) {
        uip_len = len;
        memcpy(&uip_buf, uip_buf_copy, uip_len);
        free(uip_buf_copy);
    }
}

void reception_hello_message(struct hello_message hello_msg) {
    LOG_DBG("Received hello message from: ");
    LOG_DBG_6ADDR(&hello_msg.source);
    LOG_DBG_("\n");

    LOG_DBG("Time estimate : %f\n", hello_msg.time_estimate_T_P);
    double tau_i_d = 1 / ((hello_msg.time_estimate_T_P + (float)1.0 * ANT_HOC_NET_T_HOP) / 2);
    LOG_DBG("tau_i_d: %f\n", tau_i_d);
    float pheromone_value = (float)((1 - ANT_HOC_NET_GAMMA) * tau_i_d);
    LOG_DBG("Pheromone value: %f\n", pheromone_value);
    add_neighbour_to_pheromone_table(hello_msg.source, pheromone_value);
}

void hello_loss_callback_function(void *pheromone_entry_ptr) {
    pheromone_entry_t *pheromone_entry = (pheromone_entry_t *) pheromone_entry_ptr;
    if (pheromone_entry != NULL) {
        ++pheromone_entry->hello_loss_counter;
        LOG_DBG("Hello loss counter from ");
        LOG_DBG_6ADDR(&pheromone_entry->neighbour);
        LOG_DBG_(" increased to: %d\n", pheromone_entry->hello_loss_counter);

        if (pheromone_entry->hello_loss_counter > ANT_HOC_NET_ALLOWED_HELLO_LOSS) {
            LOG_DBG("Hello loss counter exceeds allowed hello loss counter.\n");
            neighbour_node_has_disappeared(pheromone_entry->neighbour);

        } else {
            LOG_DBG("Restart hello loss timer.\n");
            ctimer_restart(&pheromone_entry->hello_timer);

            //ctimer_reset(&pheromone_entry->hello_timer);
            LOG_DBG("Timer restarted!\n");
        }
    } else {
        LOG_ERR("Pheromone entry is NULL\n");
    }
}

/*----End-Proactive-path-probing,-maintenance-and-exploration---------------------------------------------------------*/

/*----Link-failures---------------------------------------------------------------------------------------------------*/

void broadcast_link_failure_notification(link_failure_notification_t link_failure_notification) {
    uip_ipaddr_t next_hop;
    uip_create_linklocal_allnodes_mcast(&next_hop);
    uip_ipaddr_copy(&next_hop, &multicast_addr);

    LOG_DBG("Broadcast link failure notification\n");

    uint16_t size_counter = sizeof(link_failure_notification_t) - sizeof(link_failure_notification_entry_t *);
    memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN], &link_failure_notification, size_counter);

    // copy all entries
    if (link_failure_notification.size_of_list_of_destinations >= 1  && link_failure_notification.entries != NULL) {
        memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN + size_counter], link_failure_notification.entries, link_failure_notification.size_of_list_of_destinations * sizeof(link_failure_notification_entry_t));
        size_counter += link_failure_notification.size_of_list_of_destinations * sizeof(link_failure_notification_entry_t);
    }

    LOG_INFO("Link failure notification broadcasted\n");

    //send_multicast_message(ICMP6_LINK_FAILURE_NOTIFICATION, next_hop, size_counter);
    uip_icmp6_send(&next_hop, ICMP6_LINK_FAILURE_NOTIFICATION, 0, size_counter);

    if (link_failure_notification.entries != NULL) {
        free(link_failure_notification.entries);
        link_failure_notification.entries = NULL;
    }
}

void neighbour_node_has_disappeared(uip_ipaddr_t neighbour_address) {
    LOG_DBG("Neighbour node has disappeared\n");

    int length_of_notification_list;
    link_failure_notification_entry_t * notification_list = creat_link_failure_notification_entries(neighbour_address, &length_of_notification_list);
    link_failure_notification_t link_failure_notification = {
        .source = host_addr,
        .failed_link = neighbour_address,
        .size_of_list_of_destinations = length_of_notification_list,
        .entries = notification_list
    };


    LOG_DBG("Link failure notification contains:\n");
    LOG_DBG("Source: ");
    LOG_DBG_6ADDR(&link_failure_notification.source);
    LOG_DBG_("\n");
    LOG_DBG("Failed link: ");
    LOG_DBG_6ADDR(&link_failure_notification.failed_link);
    LOG_DBG_("\n");

    LOG_DBG("Size of list of destinations: %d\n", link_failure_notification.size_of_list_of_destinations);
    for (int i = 0; i < link_failure_notification.size_of_list_of_destinations; ++i) {
        LOG_DBG("Destination: ");
        LOG_DBG_6ADDR(&link_failure_notification.entries[i].uip_address_of_destination);
        LOG_DBG_("\n");
        LOG_DBG("New pheromone value: %f, New hops count: %d\n", link_failure_notification.entries[i].time_estimate_T_P_of_new_best_destination, link_failure_notification.entries[i].number_of_hops_to_new_best_destination);

    }

    //broadcast_link_failure_notification(link_failure_notification);

    if (length_of_notification_list != 0) {
        broadcast_link_failure_notification(link_failure_notification);
    } else if (notification_list != NULL) {
        free(notification_list);
        notification_list = NULL;
    }
    delete_neighbour_from_pheromone_table(neighbour_address);
    delete_neighbour_from_best_ant_array(neighbour_address);
}

void reception_link_failure_notification(link_failure_notification_t link_failure_notification) {
    LOG_DBG("Received link failure notification\n");

    int length_of_notification_list;
    // update pheromone values
    link_failure_notification_entry_t *link_failure_notification_entries_new = update_pheromone_after_link_failure(link_failure_notification, &length_of_notification_list);

    // check whether node has also lost best paths
    link_failure_notification_t link_failure_notification_new = {
            .source = host_addr,
            .failed_link = link_failure_notification.failed_link,
            .size_of_list_of_destinations = length_of_notification_list,
            .entries = link_failure_notification_entries_new,
    };

    if (length_of_notification_list != 0) {
        // only broadcast a message if the best paths are lost
        broadcast_link_failure_notification(link_failure_notification_new);
    }

    // free old list
    if (link_failure_notification.entries != NULL) {
        free(link_failure_notification.entries);
        link_failure_notification.entries = NULL;
    }
}

void data_transmission_to_neighbour_has_failed(uip_ipaddr_t destination, uip_ipaddr_t neighbour) {

    void * data = malloc(sizeof(uip_ipaddr_t)*2);
    memcpy(data, &destination, sizeof(uip_ipaddr_t));
    memcpy(((uip_ipaddr_t *)data) + 1, &neighbour, sizeof(uip_ipaddr_t));
    process_start(&data_transmission_failed_proc, (process_data_t *) data);

    if (data != NULL) {
        free(data);
    }
}

void no_pheromone_value_found_while_data_transmission(uip_ipaddr_t last_hop, uip_ipaddr_t destination) {
    struct warning_message wm = {
        .packet_type = WARNING_MESSAGE,
        .source = host_addr,
        .destination = destination
    };


    LOG_INFO("Warning message sent with destination: ");
    LOG_INFO_6ADDR(&wm.destination);
    LOG_INFO_("\n");

    memcpy(&uip_buf[UIP_IPH_LEN + UIP_ICMPH_LEN], &wm, sizeof(wm));
    uip_icmp6_send(&last_hop, ICMP6_WARNING_MESSAGE, 0, sizeof(wm));
}

void reception_warning(struct warning_message message) {
    LOG_DBG("Received warning message\n");

    delete_destination_from_pheromone_table(message.destination, message.source);
}

/*----End-Link-failures-----------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/*----FOR-ROUTING-DRIVER----------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------------------*/

/**
 * Initializes routing protocol
 */
static void
init(void)
{
    if (!initialized) {
        LOG_DBG("Routing init started!\n");
        uip_ip6addr(&uip_zeroes_addr, 0, 0, 0, 0, 0, 0, 0, 0);

        running_average_T_i_mac = (float)0.0;
        ant_generation = 0;

        best_ants = NULL;
        uip_ds6_addr_t *lladdr = uip_ds6_get_link_local(ADDR_PREFERRED);
        uip_ds6_addr_t *addr = uip_ds6_get_global(ADDR_PREFERRED);
        // get host ip addr
        if (lladdr != NULL) {
            uip_ipaddr_t global_addr;
            uip_ipaddr_copy(&global_addr, &lladdr->ipaddr);
            uip_ds6_set_addr_iid(&global_addr, &uip_lladdr);

            // if addr is null we are in the simulation
            if (addr == NULL) {
                global_addr.u16[0] = UIP_HTONS(0x2001);
                global_addr.u16[1] = UIP_HTONS(0xdb8);
                global_addr.u16[2] = UIP_HTONS(0x0);
                global_addr.u16[3] = UIP_HTONS(0x0);

                uip_ds6_addr_add(&global_addr, 0, ADDR_AUTOCONF);
            }

            lladdr = uip_ds6_get_global(ADDR_PREFERRED);
            host_addr = lladdr->ipaddr;
            LOG_INFO("Host IP address: ");
            LOG_INFO_6ADDR(&host_addr);
            LOG_INFO_("\n");

            uip_ip6addr(&multicast_addr, 0xFF03,0,0,0,0,0,0,0xfc);
            uip_ds6_maddr_add(&multicast_addr);

        } else {
            LOG_ERR("No lladdr found -> host_addr is NULL\n");
        }


        last_package_data.buffer = NULL;
        last_package_data.len = 0;
        last_package_data.destination = uip_zeroes_addr;
        last_package_data.selected_nexthop = uip_zeroes_addr;

        last_destination_data = NULL;

        buffer.valid = false;
        buffer.number_of_packets = 0;
        buffer.packet_buffer = NULL;

        pheromone_table_init();

        anthocnet_icmpv6_register_input_handlers();

        acceptance_messages = true;
        initialized = true;
        return;
    }

    if (!hello_message_broadcasting) {
#if MAC_CONF_WITH_TSCH
        if (tsch_is_associated) {
            start_broadcast_of_hello_messages();
        }
#else
        start_broadcast_of_hello_messages();
#endif
    }
    LOG_DBG("Routing init finished!\n");
}

/**
 * Usually sets the prefix for node that will operate as root.\n
 * Here nothing happens due to non-existence of root.
 * @param prefix
 * @param iid
 */
static void
root_set_prefix(uip_ipaddr_t *prefix, uip_ipaddr_t *iid)
{
    // Not used.
}

/**
 * Usually sets the node as root node and starts the network.\n
 * Does nothing here.
 * @return Always 0
 */
static int
root_start(void)
{
    return 0;
}

/**
 * Is node a root node. Returns always 0, due to decentralized routing.
 * @return Always 0
 */
static int
node_is_root(void)
{
    return 0;
}

/**
 * Usually returns the IPv6 address of the RPL root.\n
 * Here it does noting, because no root exists.
 * @param ipaddr
 * @return Always 0
 */
static int
get_root_ipaddr(uip_ipaddr_t *ipaddr)
{
    return 0;
}

/**
 * Returns the global IPv6 address of a source routing (sr) node.\n
 * Here not used.
  * @param addr
 * @param node
 * @return Always 0
 */
static int
get_sr_node_ipaddr(uip_ipaddr_t *addr, const uip_sr_node_t *node)
{
    return 0;
}

/**
 * Leaves the network the node is part of.
 */
static void
leave_network(void)
{
    LOG_DBG("Routing leave network started!\n");
    stop_broadcast_of_hello_messages();
    stop_reactive_path_setup_and_data_transmission_failed_process();
    delete_pheromone_table();
    running_average_T_i_mac = (float)0.0;
    ant_generation = 0;
    delete_best_ants_array();
    discard_buffer();
    if (last_package_data.buffer != NULL) {
        free(last_package_data.buffer);
    };
    last_package_data.buffer = NULL;
    delete_last_destination_data_array();
    acceptance_messages = false;
    LOG_DBG("Routing left network successfully!\n");
}

/**
 * Tells whether the node is currently associated to a network.
 * @return 1 if node is associated
 */
static int
node_has_joined(void)
{
    if (initialized) {
        return 1;
    }
    return 0;
}

/**
 * Tells whether the node is reachable as part of the network.\n
 * Node is reachable if initialized
 * @return 1 if node is reachable, 0 otherwise
 */
static int
node_is_reachable(void)
{
    if (initialized && neighbours_exists()) {
        return 1;
    }

    return 0;
}

/**
 * Triggers global topology repair.\n
 * Not applicable in AntHocNet.
 * @param str
 */
static void
global_repair(const char *str)
{
    // Not used. NETSTACK_ROUTING.global_repair is not called outside rpl scope.
}

/**
 * Triggers a RPL local topology repair.
 * Not applicable in AntHocNet.
 * @param str
 */
static void
local_repair(const char *str)
{
    // Not used. NETSTACK_ROUTING.local_repair is not called outside rpl scope.
}

/**
 * Removes all extension headers that pertain to the routing protocol.\n
 * There are no additional extension headers added for AntHocNet, so no need to delete any. \n
 * The current code comes from the Nullnet implementation.
 * @return true in case of success, false otherwise
 */
static bool
ext_header_remove(void)
{
#if NETSTACK_CONF_WITH_IPV6
    return uip_remove_ext_hdr();
#else
    return true;
#endif //NETSTACK_CONF_WITH_IPV6
}

/**
 * Adds/updates routing protocol extension headers to the current uIP packet.\n
 * No extensions are added for AntHocNet.
 * @return 1 in case of success
 */
static int
ext_header_update(void)
{
    return 1;
}

/**
 * Process and update the routing protocol hop-by-hop extension headers of the current uIP packet.\n
 * AntHocNet doesn't use hop-by-hop extension headers
 * @param ext_buf Pointer to the next header buffer
 * @param opt_offset The offset within the extension header where the option starts
 * @return 1 in case the packet is valid and to be processed further, 0 in case the packet must be dropped.
 */
static int
ext_header_hbh_update(uint8_t *ext_buf, int opt_offset)
{
    return 1;
}

/**
 * Process and update SRH in-place, i.e. internal address swapping as per RFC6554.
 * Is not needed for AntHocNet.
 * @return Always 0
 */
static int
ext_header_srh_update(void)
{
    // Not used. (Source Routing Header was introduced in RFC6554 for RPL)
    return 0;
}

/**
 * Look for the next hop from SRH of the current uIP packet.\n
 * Extension headers are not used in AntHocNet, but it is the first function called from the tcpip stack
 * (in the get_nexthop function), so that the logic of the routing can be put here.
 * @param ipaddr A pointer to the address where to store the next hop
 * @return 1 if a next hop was found, 0 otherwise
 */
static int
ext_header_srh_get_next_hop(uip_ipaddr_t *ipaddr)
{
    LOG_DBG("In ext header srh get next hop\n");
    // if the package is of an ant type, just sent it to its specified address
    if (UIP_ICMP_BUF->type == ICMP6_REACTIVE_FORWARD_ANT ||
        UIP_ICMP_BUF->type == ICMP6_REACTIVE_BACKWARD_ANT ||
        UIP_ICMP_BUF->type == ICMP6_PROACTIVE_FORWARD_ANT ||
        UIP_ICMP_BUF->type == ICMP6_HELLO_MESSAGE ||
        UIP_ICMP_BUF->type == ICMP6_WARNING_MESSAGE ||
        UIP_ICMP_BUF->type == ICMP6_LINK_FAILURE_NOTIFICATION) {
        *ipaddr = UIP_IP_BUF->destipaddr;
        LOG_DBG("Package is icmp of anthocnet-icmpv6 type: %d to addr: ", UIP_ICMP_BUF->type);
        LOG_DBG_6ADDR(ipaddr);
        LOG_DBG_("\n");
        return 1;
    }

    uip_ipaddr_t destination_addr = UIP_IP_BUF->destipaddr;
    return stochastic_data_routing(destination_addr, ipaddr);
}

/**
 * Called by lower layers (6LowPAN) after every packet transmission
 * @param addr The link-layer address of the packet destination
 * @param status The transmission status (os/net/mac/mac.h)
 * @param numtx The total number of transmission attempts
 */
static void
link_callback(const linkaddr_t *addr, int status, int numtx)
{
    if (status == MAC_TX_OK) {
        LOG_DBG("Link callback - Packet successfully sent!\n");
        // neighbour exists, call the function to reset the timer, if the transmission was successful
        if (!uip_ipaddr_cmp(&last_package_data.selected_nexthop, &uip_zeroes_addr)) {
            reset_hello_loss_timer(last_package_data.selected_nexthop);
        }
        return;
    }

    // if a transmission failed
    if (status != MAC_TX_DEFERRED)
    {
        LOG_DBG("Link callback - Transmission failed!\n");
        // if the buffer is empty, the last message did contain ants, so dont check those messages
        if (last_package_data.buffer == NULL) {
            LOG_DBG("Last package buffer is null -> so an ants was sent\n");
            return;
        }
        int neighbour_size = 0;
        // try to get another neighbour
        uip_ipaddr_t *neighbours = get_neighbours_to_send_to_destination(last_package_data.destination, false, &neighbour_size);
        // if no neighbour is found, call data transmission has failed
        if (neighbour_size == 0) {
            if (neighbours != NULL) {
                free(neighbours);
                neighbours = NULL;
            }
            LOG_DBG("No neighbour was found to send package to destination\n");
            data_transmission_to_neighbour_has_failed(last_package_data.destination, last_package_data.selected_nexthop);
            return;
        }
        // if neighbours are found, check if a new neighbour was found, if so send the package to this neighbour
        if (neighbours != NULL) {
            for (int i = 0; i < neighbour_size; ++i) {
                uip_ipaddr_t neighbour = neighbours[i];
                if (uip_len == 0 && !uip_ipaddr_cmp(&neighbour, &last_package_data.selected_nexthop) && last_package_data.buffer != NULL) {
                    LOG_DBG("New neighbour was found to send package to destination\n");
                    memcpy(&uip_buf, last_package_data.buffer, last_package_data.len);
                    uip_len = last_package_data.len;
                    free(last_package_data.buffer);
                    last_package_data.buffer = NULL;
                    tcpip_ipv6_output();
                    return;
                }
            }
        }
    }
}

/**
 * Called by uIP to notify addition/removal of IPv6 neighbor entries
 * \param nbr The neighbour entry
 */
static void
neighbor_state_changed(uip_ds6_nbr_t *nbr)
{
    /*
    LOG_DBG("Neighbor state changed! State: %d\n", nbr->state);
    // if the neighbour is not reachable
    if (nbr->state != NBR_REACHABLE) {
        LOG_DBG("Neighbor state changed - Neighbour is not reachable\n");
        neighbour_node_has_disappeared(nbr->ipaddr);
    }*/
}

/**
 * Called by uIP if it has decided to drop a route.\n
 * Not needed also, the route list is set to zero for AntHocNet.
 * @param route
 */
static void
drop_route(uip_ds6_route_t *route)
{
    // Not used, and not possible to be called, because of the route table size of 0.
}

/**
 * Usually tells whether the protocol is in leaf mode.\n
 * In AntHocNet every node is a none leaf node.
 * \return Always returns 0
 */
static uint8_t
is_in_leaf_mode(void)
{
    // Not used. / Every node is in none leaf mode.
    return 0;
}

const struct routing_driver anthocnet_driver = {
        "anthocnetrouting",
        init,
        root_set_prefix,
        root_start,
        node_is_root,
        get_root_ipaddr,
        get_sr_node_ipaddr,
        leave_network,
        node_has_joined,
        node_is_reachable,
        global_repair,
        local_repair,
        ext_header_remove,
        ext_header_update,
        ext_header_hbh_update,
        ext_header_srh_update,
        ext_header_srh_get_next_hop,
        link_callback,
        neighbor_state_changed,
        drop_route,
        is_in_leaf_mode,
};