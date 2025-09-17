//
// Created by thomas on 5/21/25.
//

/**
 * \file
 *      Defines new functions and types for the usage of ICMPV6 messages in AntHocNet.
 */

#ifndef ANTHOCNET_ICMPV6_H
#define ANTHOCNET_ICMPV6_H

#define ICMP6_REACTIVE_FORWARD_ANT 230
#define ICMP6_REACTIVE_BACKWARD_ANT 231
#define ICMP6_PROACTIVE_FORWARD_ANT 232
#define ICMP6_HELLO_MESSAGE 233
#define ICMP6_WARNING_MESSAGE 234
#define ICMP6_LINK_FAILURE_NOTIFICATION 235

/**
 * Registers the input handler functions for ICMPv6 messages.
 */
void anthocnet_icmpv6_register_input_handlers();

#endif //ANTHOCNET_ICMPV6_H
