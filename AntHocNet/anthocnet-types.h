/**
 * \file
 *      Type declaration for AntHocNet.
 *      Paper can be found here (https://onlinelibrary.wiley.com/doi/10.1002/ett.1062)
 */

#ifndef IEEE_802_15_4_ANTNET_ANTHOCNET_TYPES_H
#define IEEE_802_15_4_ANTNET_ANTHOCNET_TYPES_H

#include "../../contiki-ng/os/net/ipv6/uip.h"

typedef unsigned int hop_t;

/**
 * Defines the type of ant.
 */
typedef enum type_of_packet {
    REACTIVE_FORWARD_ANT,
    PATH_REPAIR_ANT,
    BACKWARD_ANT,
    WARNING_MESSAGE
} packet_type_t;

/*--Message-types-----------------------------------------------------------------------------------------------------*/
/**
 * Defines a reactive forward ant F^s_d and a path repair ant.
 */
struct reactive_forward_or_path_repair_ant {
    packet_type_t ant_type;         // the typ of ant
    unsigned int ant_generation;    // which ant generation that ant corresponds to
    uip_ipaddr_t source;            // source address of the ant
    uip_ipaddr_t destination;       // destination address of the ant
    float time_estimate_T_P;        // travel time
    hop_t number_broadcasts;        // number of broadcasts for path repair ant
    hop_t hops;                     // number of hops / length of the path
    uip_ipaddr_t* path;             // script P, path of taken nodes
};

/**
 * Defines a reactive backward ant.
 */
struct reactive_backward_ant {
    packet_type_t ant_type;         // the typ of ant
    unsigned int ant_generation;    // which ant generation that ant corresponds to
    uip_ipaddr_t destination;       // the uip addr of the node that expects the backward ant
    hop_t current_hop;              // the current hop in the path; beginning from 0 (i.e. destination)
    float time_estimate_T_P;        // estimate of travel time for a data packet
    uint8_t length;                 // length of path P / array
    uip_ipaddr_t* path;             // script P, path the ant needs to take
};

/**
 * Defines a proactive forward ant.
 */
struct proactive_forward_ant {
    uip_ipaddr_t source;            // source of the PFA
    uip_ipaddr_t destination;       // destination of the PFA
    uint8_t number_of_broadcasts;   // the number of times the ant got broadcast
    hop_t hops;                     // number of hops the ant has taken / length of the path
    uip_ipaddr_t* path;             // path the ant has take
};

/**
 * Defines hello package that only contains the nodes ip address. Sent to signal that the node is alive.
 */
struct hello_message {
    uip_ipaddr_t source;     // ip-address of the sender
    float time_estimate_T_P; // time estimate of the path to the destination
};

/**
 * Defines the necessary elements of the link notification. The address of the best destination, the number of hops to reach
 * the destination via a different path and the time estimat for that path.
 */
typedef struct link_failure_notification_entry {
    uip_ipaddr_t uip_address_of_destination;            // the uIP address of the destination
    hop_t number_of_hops_to_new_best_destination;       // the number of hops to reach the destination via the new best path
    float time_estimate_T_P_of_new_best_destination;    // the time estimate of the new best path to the destination
} link_failure_notification_entry_t ;

/**
 * Defines a link failure notification which is sent when a neighbour is assumed as disappeared.
 */
typedef struct link_failure_notification {
    uip_ipaddr_t source;                        // Source of the link failure notification
    uip_ipaddr_t failed_link;                   // uIP address of the neighbour that is lost
    uint8_t size_of_list_of_destinations;       // Size of list_of_destinations
    link_failure_notification_entry_t *entries; // List of destinations to which the node lost its best path
} link_failure_notification_t;

/**
 * Defines a warning message, used if data packets can't be routed due to missing pheromone values.
 */
struct warning_message {
    packet_type_t packet_type;      // type of the ant
    uip_ipaddr_t destination;       // the destination to which the path is lost
    uip_ipaddr_t source;            // the neighbour from which the message is sent
};

/*--End-message-types-------------------------------------------------------------------------------------------------*/

/*--Other-structs-----------------------------------------------------------------------------------------------------*/
/**
 * Structure for best ant of a generation.
 */
typedef struct best_ant {
    unsigned int generation;        // the generation of the ants
    hop_t hop_count;                // best hop count of this generation
    float time_estimate;            // best time estimate of this generation
    unsigned int first_hops_len;    // the number of elements in the first hop array
    uip_ipaddr_t *first_hops;       // first hops of the accepted ants for the acceptance for new ant with respect to a2
} best_ant_t;

/**
 * Structure for the best ant array for one neighbour.
 */
typedef struct best_ants {
    struct best_ants * next;                                // the next element in the list
    uip_ipaddr_t source;                                    // the source of the ant
    best_ant_t * best_ants_per_generation_array;            // the best ants per generation coming from the specific neighbour
    unsigned int size_of_best_ants_per_generation_array;    // the size of the best_ants_per_generation_array
} best_ants_t;

/**
 * Struct to store last package data.
 */
typedef struct last_package_data {
    uip_ipaddr_t destination;
    uip_ipaddr_t selected_nexthop;
    uint16_t len;
    unsigned char *buffer;
} last_package_data_t;

/**
 * Struct to store the times of the last packages per destination
 */
typedef struct last_destination_data {
    struct last_destination_data *next;
    uip_ipaddr_t destination;
    clock_time_t time;
    uint8_t count;
} last_destination_data_t;

/**
 * Struct to buffer uip packages
 */
typedef struct buffer_ip_packages {
    struct buffer_ip_packages *next;
    unsigned char *buffer;
    uint16_t len;
} packet_buffer_t;

/**
 * Struct to buffer package in reactive path setup
 */
typedef struct buffer {
    bool valid;
    uint16_t number_of_packets;
    packet_buffer_t *packet_buffer;
} buffer_t;

/*--End-other-structs-------------------------------------------------------------------------------------------------*/

#endif //IEEE_802_15_4_ANTNET_ANTHOCNET_TYPES_H
