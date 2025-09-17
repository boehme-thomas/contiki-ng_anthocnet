/**
 * \file
 *      Implements function for accessing and changing the pheromone table.
 */

#include "anthocnet-pheromone.h"
#include "anthocnet-conf.h"
#include "anthocnet-types.h"
#include "anthocnet.h"
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// logging
#include "sys/log.h"
#define LOG_MODULE "AntHocNet-Pheromone"
#ifdef LOG_CONF_LEVEL_ANTHOCNET_PHEROMONE
#define LOG_LEVEL LOG_CONF_LEVEL_ANTHOCNET_PHEROMONE
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif


pheromone_entry_t *pheromone_table;

pheromone_entry_t* get_pheromone_tabel_head();

void pheromone_table_init() {
    pheromone_table = NULL;
}

void delete_pheromone_table() {
    pheromone_entry_t *table = get_pheromone_tabel_head();
    while (table != NULL) {
        // delete destinations of neighbour
        destination_info_t *destination_info = table->destination_entry;
        while (destination_info != NULL)
        {
            destination_info_t *temp_dest= destination_info;
            destination_info = destination_info->next;
            free(temp_dest);
        }

        // delete pheromone entry
        pheromone_entry_t *temp = table;
        ctimer_stop(&table->hello_timer);
        table = table->next;
        free(temp);
    }
    pheromone_table_init();
}

/**
 * Returns the first pheromone table entry
 * @return First pheromone table entry
 */
pheromone_entry_t* get_pheromone_tabel_head() {
    return pheromone_table;
}

void print_pheromone_table() {
    pheromone_entry_t *tabel = get_pheromone_tabel_head();
    while(tabel != NULL)
    {
        LOG_INFO("Neighbour: ");
        LOG_INFO_6ADDR(&tabel->neighbour);
        LOG_INFO_("\n");
        destination_info_t *dest_entry = tabel->destination_entry;

        while (dest_entry != NULL) {
            LOG_INFO_(" - Destination: ");
            LOG_INFO_6ADDR(&dest_entry->destination);
            LOG_INFO_(" with pheromone value: %f, hops: %d\n", dest_entry->pheromone_value, dest_entry->hops);
            dest_entry = dest_entry->next;
        }
        tabel = tabel->next;
    }
}

/**
 * Calculates the probability that neighbour n to destination d is picked.\n
 * Equation (1) of AntHocNet paper
 * @param pheromone_value Pheromone value of the path over n to d
 * @param sum_of_pheromones_of_neighbours Sum of pheromone values of all neighbours where a path to d exists
 * @param beta Beta, needed for the equation
 * @return
 */
double calc_Pnd(float pheromone_value, float sum_of_pheromones_of_neighbours, int beta) {
    LOG_DBG("calc_Pnd: pheromone_value %f, sum_of_pheromone_of_neighbours %f, beta %d\n", pheromone_value, sum_of_pheromones_of_neighbours, beta);

    double power = pow(pheromone_value, beta);

    // if pheromone value 0 but sum also 0 then a neighbour was found
    float P_nd = (float)1.0;
    if (sum_of_pheromones_of_neighbours > 0.0) {
        P_nd = (float)(power / sum_of_pheromones_of_neighbours);
    }

    return P_nd;
}

bool neighbours_exists() {
    pheromone_entry_t *tabel = get_pheromone_tabel_head();
    if (tabel != NULL) {
        return true;
    }
    return false;
}

bool does_neighbour_exists(uip_ipaddr_t neighbour_addr) {
    pheromone_entry_t *tabel = get_pheromone_tabel_head();
    while(tabel != NULL) {
        if (uip_ipaddr_cmp(&neighbour_addr, &tabel->neighbour)) {
            return true;
        }
        tabel = tabel->next;
    }
    return false;
}

uip_ipaddr_t* get_neighbours_to_send_to_destination(uip_ipaddr_t destination, bool forward_ant, int* accepted_neighbour_size) {
    LOG_DBG("Get neighbours to send to destination: ");
    LOG_DBG_6ADDR(&destination);
    LOG_DBG_("\n");
    float sum_of_pheromone_of_neighbours = (float)0.0;
    pnd_neighbours_t *head = (pnd_neighbours_t *)malloc(sizeof(pnd_neighbours_t));
    // select beta on whether data or an ant is going to be sent
    int beta = forward_ant ? ANT_HOC_NET_BETA_FORWARD : ANT_HOC_NET_BETA_STOCHASTIC;

    head->length = 0;

    pheromone_entry_t *pheromone_head = get_pheromone_tabel_head();
    pheromone_entry_t *table = pheromone_head;

    // loop over neighbours
    while(table != NULL) {
        destination_info_t *dest_entry = table->destination_entry;
        LOG_DBG("Neighbour: ");
        LOG_DBG_6ADDR(&table->neighbour);
        LOG_DBG_("\n");
        // loop over destinations and search for the destination
        while(dest_entry != NULL) {
            LOG_DBG("Destination entry with ip address: ");
            LOG_DBG_6ADDR(&dest_entry->destination);
            LOG_DBG_("\n");
            if (uip_ipaddr_cmp(&destination, &dest_entry->destination)) {
                // add pheromone value to sum, if destination is found in the table
                sum_of_pheromone_of_neighbours += (float)pow(dest_entry->pheromone_value, beta);

                // add new neighbour and pheromone to pnd_table to access that information more efficiently later
                if (head->length == 0) {
                    head->pheromone_entry_to_the_destination = dest_entry->pheromone_value;
                    head->neighbour = table->neighbour;
                    head->next = NULL,
                    ++head->length;
                } else  {
                    pnd_neighbours_t *new = (pnd_neighbours_t *)malloc(sizeof(pnd_neighbours_t));
                    new->next = head;
                    new->neighbour = table->neighbour;
                    new->pheromone_entry_to_the_destination = dest_entry->pheromone_value;
                    new->length = head->length + 1;
                    head = new;
                }

                LOG_DBG("Destination found with pheromone value: %f! Break out of the loop -> stop printing addresses!\n", dest_entry->pheromone_value);
                // since there is just one entry for one destination, we can break out of the second loop
                break;
            }
            dest_entry = dest_entry->next;
        }

        table = table->next;
    }

    LOG_DBG("Sum of pheromone values of neighbours: %f, length: %d\n", sum_of_pheromone_of_neighbours, head->length);

    // head points to the first element of the pnd_neighbour_t list, that
    // now contains all neighbours, where pheromone values to the destination exist

    if (head->length == 0) {
        // no destinations were found -> free head pointer
        free(head);
        head = NULL;
        *accepted_neighbour_size = 0;
        return NULL;
    }

    uip_ipaddr_t *accepted_neighbours = (uip_ipaddr_t *)malloc(0 * sizeof(uip_ipaddr_t));
    *accepted_neighbour_size = 0;

    //srand(time(NULL));

    // calculate the probabilities pnds, order them increasing, calc cumulative sum
    int pnd_entry_len = head->length;
    double cumulative_probs[pnd_entry_len];
    int cumulative_prob_counter = 0;

    // first element of the list
    pnd_neighbours_t *pnd_entry = head;
    while (pnd_entry != NULL) {

        double pnd = calc_Pnd(pnd_entry->pheromone_entry_to_the_destination, sum_of_pheromone_of_neighbours, beta);

        LOG_DBG("Pnd: %f\n", pnd);

        // calculate cumulative sum to pick the neighbours accordingly
        if (cumulative_prob_counter == 0) {
            cumulative_probs[cumulative_prob_counter] = pnd;
        } else {
            cumulative_probs[cumulative_prob_counter] = cumulative_probs[cumulative_prob_counter - 1] + pnd;
        }
        cumulative_prob_counter++;
        pnd_entry = pnd_entry->next;
    }

    double rand_number = (double) rand() / RAND_MAX;
    cumulative_prob_counter = 0;
    while (head != NULL) {
        // check whether the entry of the cumulative sum is greater than the random number,
        // if that is the case, the neighbour was selected
        // that should be true for one node
        // after that use the loop to free the space
        LOG_DBG("Random number %f vs cumulative probability %f\n", rand_number, cumulative_probs[cumulative_prob_counter]);
        if (rand_number <= cumulative_probs[cumulative_prob_counter]) {

            (*accepted_neighbour_size)++;
            uip_ipaddr_t *bigger_array = (uip_ipaddr_t *) realloc(accepted_neighbours, *accepted_neighbour_size * sizeof(uip_ipaddr_t));
            if (bigger_array != NULL) {
                accepted_neighbours = bigger_array;
                LOG_DBG("Accepted neighbour: ");
                LOG_DBG_6ADDR(&head->neighbour);
                LOG_DBG_(".\n");
                accepted_neighbours[*accepted_neighbour_size - 1] = head->neighbour;
            }
        }
        cumulative_prob_counter++;

        // free pnd_entry list
        pnd_neighbours_t* temp = head;
        head = head->next;
        free(temp);
    }

    LOG_DBG("Accepted neighbours size: %d\n", *accepted_neighbour_size);
    return accepted_neighbours;
}

float* get_pheromone_value(uip_ipaddr_t neighbour, uip_ipaddr_t destination) {
    pheromone_entry_t *table = get_pheromone_tabel_head();
    // find neighbour
    while(table != NULL) {
        if (uip_ipaddr_cmp(&neighbour, &table->neighbour)) {
            destination_info_t *dest_entry = table->destination_entry;
            // find destination
            while(dest_entry != NULL) {
                if (uip_ipaddr_cmp(&destination, &dest_entry->destination)) {
                    return &table->destination_entry->pheromone_value;
                }
                dest_entry = dest_entry->next;
            }
            // return NULL if no destination is found
            return NULL;
        }
        table = table->next;
    }
    // return NULL if neighbour is not found
    return NULL;
}

hop_t* get_hops(uip_ipaddr_t neighbour, uip_ipaddr_t destination) {
    pheromone_entry_t *table = get_pheromone_tabel_head();
    // find neighbour
    while(table != NULL) {
        if (uip_ipaddr_cmp(&neighbour, &table->neighbour)) {
            destination_info_t *dest_entry = table->destination_entry;
            // find destination
            while(dest_entry != NULL) {
                if (uip_ipaddr_cmp(&destination, &dest_entry->destination)) {
                    return &table->destination_entry->hops;
                }
                dest_entry = dest_entry->next;
            }
            // return NULL if no destination is found
            return NULL;
        }
        table = table->next;
    }
    // return NULL if neighbour is not found
    return NULL;
}

void create_or_update_pheromone_table(struct reactive_backward_ant ant) {
    LOG_DBG("Update pheromone table!\n");

    // equation (5)
    double tau_i_d = 1 / ((ant.time_estimate_T_P + (float)ant.current_hop * ANT_HOC_NET_T_HOP) / 2);

    // hop to look at is no the current hop but the hop before that; makes minus two in total
    hop_t hop_to_look_at = ant.current_hop - 1 < 0 ? 0 : ant.current_hop - 1;

    uip_ipaddr_t path_neighbour = ant.path[hop_to_look_at];
    LOG_DBG("Path neighbour: ");
    LOG_DBG_6ADDR(&path_neighbour);
    LOG_DBG_(".\n");
    uip_ipaddr_t destination = ant.path[0];

    // neighbour, that is hop of now - 1
    float *pheromone_value_T_i_nd = get_pheromone_value(path_neighbour, destination);

    // no neighbour or no destination is found
    if (pheromone_value_T_i_nd == NULL) {
        LOG_DBG("No neighbour or no destination is found!\n");
        pheromone_entry_t *head = get_pheromone_tabel_head();
        pheromone_entry_t *table = head;

        // a new destination is needed anyway
        destination_info_t *new_destination = (destination_info_t*)malloc(sizeof(destination_info_t));
        new_destination->pheromone_value = (float)((1 - ANT_HOC_NET_GAMMA) * tau_i_d);
        new_destination->destination = destination;
        new_destination->hops = hop_to_look_at;
        new_destination->next = NULL;
        LOG_DBG("Created new destination entry with destination: ");
        LOG_DBG_6ADDR(&destination);
        LOG_DBG_(".\n");

        // check if the neighbour is existent, if so add the new destination entry to the neighbour
        while(table != NULL) {
            // if a neighbour is found, add new destination entry to the neighbour
            if (uip_ipaddr_cmp(&table->neighbour, &path_neighbour)) {
                LOG_DBG("Path neighbour exists - add destination.\n");

                new_destination->next = table->destination_entry;
                // add new entry for current destination
                table->destination_entry = new_destination;

                // when added stop
                return;
            }
            table = table->next;
        }

        // if reached then the neighbour is not yet created
        LOG_DBG("Neighbour ");
        LOG_DBG_6ADDR(&path_neighbour);
        LOG_DBG_(" not yet in pheromone table - add new entry.\n");
        pheromone_entry_t *new_entry = (pheromone_entry_t *)malloc(sizeof(pheromone_entry_t));
        new_entry->neighbour = path_neighbour;
        new_entry->destination_entry = new_destination;
        new_entry->next = head;
        ctimer_set(&new_entry->hello_timer, ANT_HOC_NET_T_HELLO_SEC * CLOCK_SECOND, hello_loss_callback_function, new_entry);
        pheromone_table = new_entry;
        return;
    }

    // destination and thus pheromone value is found
    // equation (6)
    *pheromone_value_T_i_nd = ANT_HOC_NET_GAMMA * (*pheromone_value_T_i_nd) + (1 - ANT_HOC_NET_GAMMA) * tau_i_d;
}

int reset_hello_loss_timer(uip_ipaddr_t neighbour_address) {
    pheromone_entry_t *head = get_pheromone_tabel_head();
    pheromone_entry_t *table = head;
    while (table != NULL) {
        // if the neighbour is found, reset its timer and hello loss counter
        if (uip_ipaddr_cmp(&table->neighbour, &neighbour_address)) {
            // reset the timer for hello loss
            ctimer_restart(&table->hello_timer);
            // set counter to 0
            table->hello_loss_counter = 0;
            LOG_DBG("Neighbour already in pheromone table - timer and count reset.\n");
            return 1;
        }
        table = table->next;
    }
    return 0;
}

void add_neighbour_to_pheromone_table(uip_ipaddr_t neighbour_address, float pheromone_value) {
    LOG_DBG("Add neighbour to pheromone table.\n");
    pheromone_entry_t *head = get_pheromone_tabel_head();

    int ret = reset_hello_loss_timer(neighbour_address);
    if (ret) {
        // the neighbour was found, timer reset, no need to add a new neighbour
        return;
    }

    LOG_DBG("Neighbour not in pheromone table - add new entry.\n");
    // new destination entry
    destination_info_t *new_destination = (destination_info_t*)malloc(sizeof(destination_info_t));
    new_destination->pheromone_value = pheromone_value;
    new_destination->destination = neighbour_address;
    new_destination->hops = 1;
    new_destination->next = NULL;

    // when arrived here, no neighbour with that uip addr is found
    pheromone_entry_t *new_entry = (pheromone_entry_t *)malloc(sizeof(pheromone_entry_t));
    new_entry->neighbour = neighbour_address;
    new_entry->destination_entry = new_destination;
    new_entry->next = head;
    new_entry->hello_loss_counter = 0;

    ctimer_set(&new_entry->hello_timer, ANT_HOC_NET_T_HELLO_SEC * CLOCK_SECOND, &hello_loss_callback_function, new_entry);
    LOG_DBG("New Neighbour added: ");
    LOG_DBG_6ADDR(&new_entry->neighbour);
    LOG_DBG_(".\n");
    pheromone_table = new_entry;
}

void delete_neighbour_from_pheromone_table(uip_ipaddr_t neighbour_address) {
    LOG_DBG("Delete neighbour from pheromone table.\n");
    pheromone_entry_t *head = get_pheromone_tabel_head();
    pheromone_entry_t *table = head;
    pheromone_entry_t *previous = NULL;

    while (table != NULL) {
        if (uip_ipaddr_cmp(&table->neighbour, &neighbour_address)) {
            destination_info_t *entry = table->destination_entry;

            // free the destination array
            while (entry != NULL) {
                destination_info_t *temp = entry;
                entry = entry->next;
                free(temp);
            }

            // set the previous pointer to the neighbour next entry
            // if table pointed to the first element of the pheromone table, that pointer must be bent
            if (previous == NULL) {
                pheromone_table = table->next;
            } else {
                previous->next = table->next;
            }

            LOG_DBG("Neighbour deleted: ");
            LOG_DBG_6ADDR(&table->neighbour);
            LOG_DBG_(".\n");
            // stop timer
            ctimer_stop(&table->hello_timer);
            // free the destination entry
            free(table);
            return;
        }
        previous = table;
        table = table->next;
    }
}

void delete_destination_from_pheromone_table(uip_ipaddr_t destination, uip_ipaddr_t neighbour) {
    LOG_DBG("Delete destination from pheromone table.\n");
    pheromone_entry_t *head = get_pheromone_tabel_head();
    pheromone_entry_t *table = head;
    while (table != NULL) {
        if(uip_ipaddr_cmp(&neighbour, &table->neighbour)) {
            destination_info_t *destination_entry = table->destination_entry;
            destination_info_t *previous_destination = NULL;

            while (destination_entry != NULL) {
                // if destination is found, delete the entry; no need to delete neighbour entry, or create a new empty
                // destination entry, since the first entry with the neighbour as destination itself always exists
                if (uip_ipaddr_cmp(&destination, &destination_entry->destination)) {
                    destination_info_t * temp = destination_entry;
                    if (previous_destination == NULL) {
                        table->destination_entry = destination_entry->next;
                    } else {
                        previous_destination->next = destination_entry->next;
                    }

                    LOG_DBG("Destination deleted: ");
                    LOG_DBG_6ADDR(&temp->destination);
                    LOG_DBG_(".\n");
                    free(temp);
                    return;
                }
                previous_destination = destination_entry;
                destination_entry = destination_entry->next;
            }
            // the given destination entry is not found
            return;
        }
        table = table->next;
    }
}

link_failure_notification_entry_t *creat_link_failure_notification_entries(uip_ipaddr_t neighbour_address, int *length_of_notification_list) {
    LOG_DBG("Create link failure notification entries.\n");
    pheromone_entry_t * head = get_pheromone_tabel_head();
    pheromone_entry_t * table = head;

    *length_of_notification_list = 0;
    link_failure_notification_entry_t * list_destinations_of_lost_neighbour = (link_failure_notification_entry_t *)malloc(*length_of_notification_list * sizeof (link_failure_notification_entry_t));

    destination_info_t * destinations_of_neighbours_head = NULL;

    // loop through pheromone table and get the pointer to the first destination entry of the searched neighbour
    while (table != NULL) {
        if (uip_ipaddr_cmp(&neighbour_address, &table->neighbour)) {
            destinations_of_neighbours_head = table->destination_entry;
            break;
        }
        table = table->next;
    }

    destination_info_t * destinations_of_neighbours = destinations_of_neighbours_head;

    // loop through the destination entries of the lost neighbour
    while(destinations_of_neighbours != NULL) {
        // to safe the new best destination
        destination_info_t *new_best_destination = NULL;

        // to be able to break out of both loops when a better destination entry than of the lost neighbour is found
        // also used to save if a better neighbour was found
        // (the check on new_best_destiantion == NULL is not enough,
        // since it's null, if no neighbour was found and if a better than the lost neighbour was found)
        bool done = false;

        table = head;

        // loop through pheromone table
        while (table != NULL) {
            // skip the entry of the lost neighbour
            if (uip_ipaddr_cmp(&neighbour_address, &table->neighbour)) {
                table = table->next;
                continue;
            }
            destination_info_t *destination_entry = table->destination_entry;

            // loop through all destination entries of the current neighbour
            while (destination_entry != NULL) {
                // continue if address of destination is not the same as the destination entry of the lost neighbour
                if (!uip_ipaddr_cmp(&destination_entry->destination, &destinations_of_neighbours->destination)) {
                    destination_entry = destination_entry->next;
                    continue;
                }
                /* check if pheromone value of current destination entry is smaller than that of the lost neighbour
                 * if smaller:      delete new_best_destination, since the destination of the lost neighbour is not the best
                 * if not smaller:  the destination of the lost neighbour might have the best path
                 *                  -> compare to new_best_destination, assign if better
                 */
                // the current destination is better than the lost one
                if (destination_entry->pheromone_value < destinations_of_neighbours->pheromone_value) {
                    // the lost destination is not the best
                    // thus no entry must be created since there is a path to the destination
                    new_best_destination = NULL;
                    // to break both inner while loops, to get to the next destination entry of the lost neighbour
                    done = true;
                    break;

                // the current destination is worse than the lost one
                } else {
                    // if new_best_destination entry is null just assign current destination entry
                    // since it is the best seen yet
                    if (new_best_destination == NULL) {
                        new_best_destination = destination_entry;
                    } else {
                        // if new_best_destination exists, check whether the pheromone value of the current is better
                        // if so save the entry, since it is the best (after the lost path)
                        if (new_best_destination->pheromone_value > destination_entry->pheromone_value) {
                            new_best_destination = destination_entry;
                        }
                    }
                }

                destination_entry = destination_entry->next;
            }
            // break out of the loop of the pheromone table
            // because a path with a better prheromone value was found, so no the best path was lost and thus must not be
            // included in the notification
            if (done) {
                break;
            }
            table = table->next;
        }

        // if done, a better neighbour path was found, thus this entry doesn't need to be included in the notification
        // coninue with the next destinatino entry
        if (done) {
            destinations_of_neighbours = destinations_of_neighbours->next;
            continue;
        }

        (*length_of_notification_list)++;

        link_failure_notification_entry_t *new_list = (link_failure_notification_entry_t *) realloc(list_destinations_of_lost_neighbour, *length_of_notification_list * sizeof(link_failure_notification_entry_t));
        if (new_list != NULL) {
            list_destinations_of_lost_neighbour = new_list;

            link_failure_notification_entry_t new_entry;
            if (new_best_destination != NULL) {
                // if not null a best path (after the lost one) was found, so save its parameters
                // memcpy in order to free the neighour and destination

                //new_entry.number_of_hops_to_new_best_destination = new_best_destination->hops;
                memcpy(&new_entry.number_of_hops_to_new_best_destination, &new_best_destination->hops, sizeof(new_best_destination->hops));
                //new_entry.time_estimate_T_P_of_new_best_destination = new_best_destination->pheromone_value;
                memcpy(&new_entry.time_estimate_T_P_of_new_best_destination, &new_best_destination->pheromone_value, sizeof(new_best_destination->pheromone_value));
                //new_entry.uip_address_of_destination = new_best_destination->destination;
                memcpy(&new_entry.uip_address_of_destination, &new_best_destination->destination, sizeof(new_best_destination->destination));
            } else {
                // if this is reached, no best path (after the lost one) was found, and no better path exists
                new_entry.number_of_hops_to_new_best_destination = 0;
                new_entry.time_estimate_T_P_of_new_best_destination = (float)-100.0;
                //new_entry.uip_address_of_destination = destinations_of_neighbours->destination;
                memcpy(&new_entry.uip_address_of_destination, &destinations_of_neighbours->destination, sizeof(destinations_of_neighbours->destination));
            }
            //list_destinations_of_lost_neighbour[*length_of_notification_list - 1] = new_entry;
            memcpy(&list_destinations_of_lost_neighbour[*length_of_notification_list - 1], &new_entry, sizeof(link_failure_notification_entry_t));
        }
        destinations_of_neighbours = destinations_of_neighbours->next;
    }

    if (*length_of_notification_list == 0) {
        if (list_destinations_of_lost_neighbour != NULL) {
            free(list_destinations_of_lost_neighbour);
            list_destinations_of_lost_neighbour = NULL;
        }
        return NULL;
    }
    return list_destinations_of_lost_neighbour;
}

destination_info_t *get_destination_entry(uip_ipaddr_t neighbour_address, uip_ipaddr_t destination_address) {
    pheromone_entry_t * head = get_pheromone_tabel_head();
    pheromone_entry_t * table = head;

    // find destination enty
    while(table != NULL) {
        if (uip_ipaddr_cmp(&neighbour_address, &table->neighbour)) {
            destination_info_t *dest_entry = table->destination_entry;
            // find destination
            while(dest_entry != NULL) {
                if (uip_ipaddr_cmp(&destination_address, &dest_entry->destination)) {
                    return dest_entry;
                }
                dest_entry = dest_entry->next;
            }
            // return NULL if no destination is found
            return NULL;
        }
        table = table->next;
    }
    // return NULL if neighbour is not found
    return NULL;
}

link_failure_notification_entry_t* create_one_link_failure_notification(uip_ipaddr_t destination, uip_ipaddr_t neighbour_address) {

    pheromone_entry_t *head = get_pheromone_tabel_head();
    pheromone_entry_t *table = head;

    destination_info_t *destination_of_neighbour = get_destination_entry(neighbour_address, destination);
    // if that destination does no exist, create no messagse
    if (destination_of_neighbour == NULL) {
        return NULL;
    }

    // to safe the new best destination
    destination_info_t *new_best_destination = NULL;

    // to be able to break out of both loops when a better destination entry than of the lost neighbour is found
    // also used to save if a better neighbour was found
    // (the check on new_best_destiantion == NULL is not enough,
    // since it's null, if no neighbour was found and if a better than the lost neighbour was found)
    bool done = false;

    // loop through pheromone table
    while (table != NULL) {
        // skip the entry of the neighbour
        if (uip_ipaddr_cmp(&neighbour_address, &table->neighbour)) {
            table = table->next;
            continue;
        }
        destination_info_t *destination_entry = table->destination_entry;

        // loop through all destination entries of the current neighbour
        while (destination_entry != NULL) {
            // continue if address of destination is not the same as the destination entry of the lost neighbour
            if (!uip_ipaddr_cmp(&destination_entry->destination, &destination)) {
                destination_entry = destination_entry->next;
                continue;
            }
            /* check if pheromone value of current destination entry is smaller than that of the lost neighbour
             * if smaller:      delete new_best_destination, since the destination of the lost neighbour is not the best
             * if not smaller:  the destination of the lost neighbour might have the best path
             *                  -> compare to new_best_destination, assign if better
             */
            // the current destination is better than the lost one
            if (destination_entry->pheromone_value < destination_of_neighbour->pheromone_value) {
                // the lost destination is not the best
                // thus no entry must be created since there is a path to the destination
                new_best_destination = NULL;
                // to break both inner while loops, to get to the next destination entry of the lost neighbour
                done = true;
                break;

            // the current destination is worse than the lost one
            } else {
                // if new_best_destination entry is null just assign current destination entry
                // since it is the best seen yet
                if (new_best_destination == NULL) {
                    new_best_destination = destination_entry;
                } else {
                    // if new_best_destination exists, check whether the pheromone value of the current is better
                    // if so save the entry, since it is the best (after the lost path)
                    if (new_best_destination->pheromone_value > destination_entry->pheromone_value) {
                        new_best_destination = destination_entry;
                    }
                }
            }

            destination_entry = destination_entry->next;
        }
        // break out of the loop of the pheromone table
        // because a path with a better prheromone value was found, so no the best path was lost and thus must not be
        // included in the notification
        if (done) {
            break;
        }
        table = table->next;
    }

    // if done, a better neighbour path was found, thus this entry doesn't need to be included in the notification
    // coninue with the next destinatino entry
    if (done) {
        return NULL;
    }

    link_failure_notification_entry_t *new_entry = malloc(sizeof(link_failure_notification_entry_t));
    if (new_entry != NULL) {

        if (new_best_destination != NULL) {
            // if not null a best path (after the lost one) was found, so save its parameters
            // memcpy in order to free the neighour and destination

            new_entry->number_of_hops_to_new_best_destination = new_best_destination->hops;
            //memcpy(&new_entry->number_of_hops_to_new_best_destination, &new_best_destination->hops, sizeof(new_best_destination->hops));
            new_entry->time_estimate_T_P_of_new_best_destination = new_best_destination->pheromone_value;
            //memcpy(&new_entry->time_estimate_T_P_of_new_best_destination, &new_best_destination->pheromone_value, sizeof(new_best_destination->pheromone_value));
            new_entry->uip_address_of_destination = new_best_destination->destination;
            //memcpy(&new_entry->uip_address_of_destination, &new_best_destination->destination, sizeof(new_best_destination->destination));
        } else {
            // if this is reached, no best path (after the lost one) was found, and no better path exists
            new_entry->number_of_hops_to_new_best_destination = 0;
            new_entry->time_estimate_T_P_of_new_best_destination = (float)-100.0;
            new_entry->uip_address_of_destination = destination_of_neighbour->destination;
            //memcpy(&new_entry->uip_address_of_destination, &destination_of_neighbour->destination, sizeof(destination_of_neighbour->destination));
        }
    }
    return new_entry;
}

link_failure_notification_entry_t * update_pheromone_after_link_failure(link_failure_notification_t link_failure_notification, int *length_of_notification_list) {
    LOG_DBG("Update pheromone after link failure.\n");
    float *pheromone_value = NULL;

    *length_of_notification_list = 0;
    link_failure_notification_entry_t * list_destinations_of_lost_neighbour = (link_failure_notification_entry_t *)malloc(*length_of_notification_list * sizeof (link_failure_notification_entry_t));

    for (int i = 0; i < link_failure_notification.size_of_list_of_destinations; i++) {

        if (link_failure_notification.entries[i].number_of_hops_to_new_best_destination == 0 && link_failure_notification.entries[i].time_estimate_T_P_of_new_best_destination == (float)-100.0) {

            link_failure_notification_entry_t *new_entry = create_one_link_failure_notification(link_failure_notification.entries[i].uip_address_of_destination, link_failure_notification.source);

            if (new_entry != NULL) {
                ++(*length_of_notification_list);
                link_failure_notification_entry_t *new_list = malloc(*length_of_notification_list * sizeof(link_failure_notification_entry_t));
                if (new_list != NULL) {
                    if (length_of_notification_list - 1 > 0) {
                        memcpy(new_list, list_destinations_of_lost_neighbour, (*length_of_notification_list - 1) * sizeof(link_failure_notification_entry_t));
                        memcpy(&new_list[*length_of_notification_list - 1], new_entry, sizeof(link_failure_notification_entry_t));
                        list_destinations_of_lost_neighbour = new_list;
                    } else {
                        free(list_destinations_of_lost_neighbour);
                        memcpy(new_list, new_entry, sizeof(link_failure_notification_entry_t));
                        list_destinations_of_lost_neighbour = new_list;
                    }
                }
                free(new_entry);
                new_entry = NULL;
            }

            // this is the case if the neighbour of the lost link has no path to the destination, and lost it
            // thus this node has to remove the destination over this link
            delete_destination_from_pheromone_table(link_failure_notification.entries[i].uip_address_of_destination, link_failure_notification.source);
        } else {
            pheromone_value = get_pheromone_value(link_failure_notification.source, link_failure_notification.entries[i].uip_address_of_destination);

            if (pheromone_value != NULL) {
                // if null, that the enrty doesnt exists
                double tau_i_d = 1 / ((link_failure_notification.entries[i].time_estimate_T_P_of_new_best_destination + (float)link_failure_notification.entries[i].number_of_hops_to_new_best_destination * ANT_HOC_NET_T_HOP) / 2);
                *pheromone_value = ANT_HOC_NET_GAMMA * (*pheromone_value) + (1 - ANT_HOC_NET_GAMMA) * tau_i_d;
            }

            hop_t *hops = get_hops(link_failure_notification.source, link_failure_notification.entries[i].uip_address_of_destination);
            if (hops != NULL) {
                *hops = link_failure_notification.entries[i].number_of_hops_to_new_best_destination;
            }
        }
    }
    if (*length_of_notification_list == 0) {
        return NULL;
    }
    return list_destinations_of_lost_neighbour;
}
