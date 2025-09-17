/**
 * \file
 *      Declarations of Functions and Structs for the pheromone table
 */
#ifndef IEEE_802_15_4_ANTNET_ANTHOCNET_PHEROMONE_H
#define IEEE_802_15_4_ANTNET_ANTHOCNET_PHEROMONE_H

#include "../../contiki-ng/os/net/ipv6/uip.h"
#include "anthocnet-types.h"

/**
 * Defines the destination containing the uIP address and pheromone entry, among other things.
 */
typedef struct destination_information {
    struct destination_information *next;   // next destination entry
    uip_ipaddr_t destination;               // ip address of the destination
    float pheromone_value;                  // pheromone_value
    hop_t hops;                             // number of hops to that destination
} destination_info_t;

/**
 * Defines the pheromone table T^i, which contains routing information.\n
 * One instance contains the neighbour address and the destination_info_t list, which contains the information of which node
 * can be reached via that neighbour.
 */
typedef struct pheromone_entry {
    struct pheromone_entry *next;           // next pheromone entry
    uip_ipaddr_t neighbour;                 // the next hop neighbour
    destination_info_t *destination_entry;  // destination information about this neighbour
    struct ctimer hello_timer;              // timer to handle the reception of hello messages
    uint8_t hello_loss_counter;             // counts the number of lost hellos
} pheromone_entry_t;

/**
 * To safe the pheromone values and neighbours for the calculation of P_nd.
 */
typedef struct pnd_neighbours {
    struct pnd_neighbours *next;                // next pnd_neighbour entry
    uip_ipaddr_t neighbour;                     // neighbour address
    float pheromone_entry_to_the_destination;   // pheromone value of that neighbour
    uint8_t length;                             // length of te pnd_neighbour list
} pnd_neighbours_t;

/**
 * Initializes the pheromone table.
 */
void pheromone_table_init();

/**
 * Deletes entire pheromone table;
 */
void delete_pheromone_table();

/**
 * Checks if pheromone table contains neighbours.
 * @return True, if neighbours exist, false otherwise
 */
bool neighbours_exists();

/**
 * Checks whether a neighbour with the give uIP address exists.
 * @param neighbour_addr The uIP address of the neighbour
 * @return 1 if the neighbour exists, 0 otherwise
 */
bool does_neighbour_exists(uip_ipaddr_t neighbour_addr);

/**
 * Prints the pheromone table to the log.
 */
void print_pheromone_table();

/**
 * Picks neighbour to which packages is sent.
 * @param destination The destination to which packages should be sent to
 * @param forward_ant Whether the calculation is for ant or for data packages
 * @param accepted_neighbour_size Pointer to store the size of the resulting array in
 * @return Pointer to a list of neighbours, or NULL if there are no neighbours
 */
uip_ipaddr_t* get_neighbours_to_send_to_destination(uip_ipaddr_t destination, bool forward_ant, int* accepted_neighbour_size);

/**
 * Return the pheromone value of the route via the neighbour to the destination.
 * \param neighbour The neighbour, which pheromone entries are looked at
 * \param destination The destination to which the pheromone value is searched for
 * \return Pointer to the pheromone value, NULL if the neighbour or destination is not found
 */
float* get_pheromone_value(uip_ipaddr_t neighbour, uip_ipaddr_t destination);

/**
 * Updates the pheromone table entry T_i_nd. Corresponds to equation (5) and (6) of the AntHocNet paper.
 * @param ant The reactive backward ant which holds the needed information
 */
void create_or_update_pheromone_table(struct reactive_backward_ant ant);

/**
 * Adds neighbour to pheromone table if not already existent; resets timer is a neighbour already exist.
 * @param neighbour_address uIP address of the neighbour
 * @param pheromone_value the time estimate of the neighbour according to equation (3) of the paper
 */
void add_neighbour_to_pheromone_table(uip_ipaddr_t neighbour_address,float pheromone_value);

/**
 * Removes neighbour (and all of its destination entries) form the routing table.
 * @param neighbour_address The address of the neighbour to be deleted
 */
void delete_neighbour_from_pheromone_table(uip_ipaddr_t neighbour_address);

/**
 * Resets the timer of the given neighbour
 * @param neighbour_address The uip address of the neighbour to reset the timer
 * @return 1 if the timer was reset, 0 if no neighbour was found
 */
int reset_hello_loss_timer(uip_ipaddr_t neighbour_address);

/**
 * Removes the path to destination d over neighbour n, more precise, it removes the destination entry d corresponding
 * to the given neighbour n, from the pheromone table.
 * @param destination The destination uIP address, which will be deleted
 * @param neighbour The neighbour uIP address, to which the destination corresponds.
 */
void delete_destination_from_pheromone_table(uip_ipaddr_t destination, uip_ipaddr_t neighbour);

/**
 * Creates entries for the link failure notification
 * @param neighbour_address Address of the lost neighbour
 * @param length_of_notification_list Number of elements in the returned list
 * @return List of link failure notification entries
 */
link_failure_notification_entry_t *creat_link_failure_notification_entries(uip_ipaddr_t neighbour_address, int *length_of_notification_list);

/**
 * Updates the pheromone table; does not create new neighbours or destination entries and return the link_failure_notification list,
 * or NULL if no entries were found.
 * Should be called after receiving a link failure notification.
 * @param link_failure_notification The received link failure notification
 * @param length_of_notification_list Where the length of the list is saved
 */
link_failure_notification_entry_t * update_pheromone_after_link_failure(link_failure_notification_t link_failure_notification, int *length_of_notification_list);
#endif //IEEE_802_15_4_ANTNET_ANTHOCNET_PHEROMONE_H
