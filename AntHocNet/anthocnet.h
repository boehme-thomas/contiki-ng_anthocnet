/**
 * \file
 *      API declaration for AntHocNet.
 *      Paper can be found here (https://onlinelibrary.wiley.com/doi/10.1002/ett.1062)
 */

#ifndef IEEE_802_15_4_ANTNET_ANTHOCNET_H
#define IEEE_802_15_4_ANTNET_ANTHOCNET_H

#include "anthocnet-types.h"
#include "../../contiki-ng/os/net/ipv6/uip.h"

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch-types.h"
#endif

//-------General functions---------

/**
 * Gets the current ant generation.
 * @return The current ant generation
 */
unsigned int get_current_ant_generation();

/**
 * Gets the host uIP address.
 * @retun The host uIP address.
 */
uip_ipaddr_t get_host_address();

/**
 * Whether the stochastic path setup process or the data transmission failed process is running.
 * @return 1 if the stochastic path setup process or the data transmission failed process is running, 0 otherwise
 */
int processes_running();

/**
 * @return 1 if the node is allowed to receive messages, 0 otherwise
 */
int accept_messages();

/**
 * Calculates the running average of time elapsed between the arrival of a packet at the MAC layer
 * and the end of a successful transmission. It is necessary for calc_time_estimate_T_P and corresponds to equation (4)
 * in the AntHocNet paper.
 * @param new_time_t_i_mac Time the node needs to send a packet
 */
void update_running_average_T_i_mac(float new_time_t_i_mac);

//-------Reactive path setup-------
/**
 * Sends reactive forward ant or path repair ant. Either broadcast or unicast to the next hop.
 * @param broadcast Whether the ant should be sent via broadcast or unicast.
 * @param next_hop The uIP of the next hop, if no broadcast is
 * @param ant The reactive forward ant to be sent.
 */
void send_reactive_forward_or_path_repair_ant(bool broadcast, uip_ipaddr_t next_hop, struct reactive_forward_or_path_repair_ant ant);

/**
 * Handles reception of reactive forward ants and path repair ants.\n
 * Either sends backward ant if the node is destination, kills ant when max hop is reached, or updates time_estimate_T_P,
 * adds the node to the path and forwards ant, when it is accepted.
 *
 * When more ants of the same generation are received, the path of the new ant is compared to the former ones. If the
 * number of hops and travel time are both within the acceptance factor ANT_HOC_NET_ACC_FACTOR_A1 of that of the best
 * ant of the generation, the ant will be forwarded. Furthermore, the acceptance factor ANT_HOC_NET_ACC_FACTOR_A2 is used
 * if the first hop is different from those taken by previously accepted ants.
 * @param ant The reactive forward or the path repair ant
 */
void reception_reactive_forward_or_path_repair_ant(struct reactive_forward_or_path_repair_ant ant);

/**
 * Creates and sends a backward ant.
 * @param ant_gen The new generation of the ant
 * @param hops The number of hops of the forward ant
 * @param path The path that the forward ant has hold
 * @param destination The uip address of the destination
 */
void create_and_send_backward_ant(unsigned int ant_gen, hop_t hops, uip_ipaddr_t * path, uip_ipaddr_t destination);

/**
 * Handles reception of reactive backward ant.\n
 * Calculates time_estimate_T_P, updates routing tables and sends it to the next hop or discards it when the hop is not
 * found.
 * @param ant The received backward ant
 */
void reception_reactive_backward_ant(struct reactive_backward_ant ant);

/**
 * Sends the package buffered by the reactive path setup or by the data transmission failed process.
 */
void send_buffered_data_packages();

/**
 * Is called when reactive path setup should be executed, i.e. if no routing information is available to reach destination d.\n
 * The path setup process is repeated if no reactive backward ant is received withing ANT_HOC_NET_RESTART_PATH_SETUP_SECS.
 * After ANT_HOC_NET_MAX_TRIES_PATH_SETUP tries, the data is discarded. This happens in a Process.
 * @param destination uIP address of the destination of data packages.
 */
void reactive_path_setup(uip_ipaddr_t destination);

/**
 * Stops reactive path setup and data transmission failed process when either of thems is running.
 */
void stop_reactive_path_setup_and_data_transmission_failed_process();

//-------End reactive path setup-------

//-------Stochastic data routing-------

/**
 * Is called when data should be sent. Starts reactive path setup if no neighbour was selected / if no neighbour is available.\n
 * Forwards data stochastically if neighbours are available. The next hop is chosen based on P_nd. \n
 * @param destination uIP address of the destination of data packages.
 * @param address The pointer where the address should be saved in
 * @return 1 if the address was found
 */
int stochastic_data_routing(uip_ipaddr_t destination, uip_ipaddr_t *address);

//-------End stochastic data routing-------

//-------Proactive path probing, maintenance and exploration-------

/**
 * Unicast to next hop, chosen by (1), or broadcast with a probability of ANT_HOC_NET_PFA_BROADCAST_PROBABILITY.\n
 * Ant is killed if the number of broadcasts extends ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PROACTIVE_FORWARD_A.
 * @param ant The proactive forward ant to be sent
 */
void send_proactive_forward_ant(struct proactive_forward_ant ant);

/**
 * Handles reception of a proactive forward ant. Node is added to the ant's path.
 * If this is the destination, a backward ant is created and sent if the ant is sent further.
 *
 * @param ant The proactive forward ant that was received.
 */
void reception_proactive_forward_ant(struct proactive_forward_ant ant);

void start_broadcast_of_hello_messages();

/**
 * Broadcasts hello message.
 */
void broadcast_hello_messages();

/**
 * Handles reception of a hello message.\n
 * If a message from a new neighbour is received, it is added to the routing tabel.
 * @param hello_msg The received hello message
 */
void reception_hello_message(struct hello_message hello_msg);

/**
 * Callback function for ctimer for the reception of hello messages.
 * @param pheromone_entry_ptr Pointer to the neighbour entry
 */
void hello_loss_callback_function(void *pheromone_entry_ptr);

//-------End proactive path probing, maintenance and exploration-------

//-------Link failures-------

/**
 * Is called when a node is assumed to be disappeared, i.e. when no hello message is received of neighbour n or when a
 * unicast transmission failed.\n
 * Removes neighbour n out of the neighbour list and associated entries from the routing table, calls broadcasts_link_failure_notification
 * @param neighbour_address The uIP address of the disappeared neighbour
 */
void neighbour_node_has_disappeared(uip_ipaddr_t neighbour_address);

/**
 * Handles reception of link failure notification.\n
 * Updates pheromone table with received new estimates, calls broadcast_link_failure_notification if the best or the only path to
 * the destination is lost.
 * @param link_failure_notification The received link failure notification
 */
void reception_link_failure_notification(link_failure_notification_t link_failure_notification);

/**
 * Is called when data transmission to a neighbour has failed and there is no other path available.\n
 * Tries to locally repair the path: Broadcasts a path repair ant, waiting for backward repair ant to arrive, if no ant has
 * arrived, buffered data packets are dropped, and a link failure notification is sent.
 * @param destination The uIP address of the destination to which the data transmission failed
 * @param neighbour The uIP address of the next hop neighbour over which the data transmission failed
 */
void data_transmission_to_neighbour_has_failed(uip_ipaddr_t destination, uip_ipaddr_t neighbour);

/**
 * Is called when a data packet can't be routed, due to missing pheromone values.\n
 * Unicasts a warning message to the last hop.
 * @param last_hop The uIP address of the last hop
 * @param destination The uIP address of the destination
 */
void no_pheromone_value_found_while_data_transmission(uip_ipaddr_t last_hop, uip_ipaddr_t destination);

/**
 * Handles reception of a warning message.\n
 * Deletes wrong routing information, i.e. the destination entry from the pheromone table
 * @param message The received warning message
 */
void reception_warning(struct warning_message message);

//-------End link failures-------

#endif //IEEE_802_15_4_ANTNET_ANTHOCNET_H