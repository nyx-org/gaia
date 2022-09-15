/*
 * Copyright (c) 2022, lg
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/** @file
 *  @brief Definitions related to ports.
 */

#ifndef SRC_GAIA_PORTS_H
#define SRC_GAIA_PORTS_H
#include <gaia/base.h>
#include <gaia/vec.h>

#define PORT_RIGHT_RECV (1 << 0)
#define PORT_RIGHT_SEND (1 << 1)

#define PORT_QUEUE_MAX 16

#define PORT_MSG_TYPE_DEFAULT (1 << 0)
#define PORT_MSG_TYPE_RIGHT (1 << 1)

#define PORT_DEAD (~0)
#define PORT_NULL (0)

#define WELL_KNOWN_PORTS_MAX 4

typedef struct PACKED
{
    uint8_t type;
    uint32_t size;
    uint32_t dest;
    uint32_t port_right;
    uint8_t port_type;
} PortMessageHeader;

typedef struct
{
    uint32_t port;
} PortMessageKernelData;

typedef struct PACKED
{
    PortMessageHeader header;
    PortMessageKernelData kernel_data;
} PortMessage;

typedef struct
{
    int head, tail, length;
    PortMessage *messages[PORT_QUEUE_MAX];
} PortQueue;

typedef struct
{
    PortQueue queue;
} Port;

// Entries of the port namespace
// For example, a port right that refers to port 1 and has RECEIVE rights would look like this:
// name=current_name++, rights=RECEIVE, port=1
typedef struct
{
    uint32_t name;  // Its name
    uint8_t rights; // Port rights

    uint32_t port; // To which port it refers to (NOTE: This is NOT seen by the user)
} PortBinding;

typedef struct
{
    Vec(PortBinding) bindings;
    int current_name;

    PortBinding well_known_ports[WELL_KNOWN_PORTS_MAX]; // Inherited through fork() syscall (send only rights)
} PortNamespace;

uint32_t port_allocate(PortNamespace *ns, uint8_t rights);

void port_send(PortNamespace *ns, PortMessageHeader *message);

PortMessageHeader *port_receive(PortNamespace *ns, uint32_t name);

void port_send_right(PortNamespace *src_ns, PortNamespace *dest_ns, uint32_t name);

void register_well_known_port(PortNamespace *ns, uint8_t index, PortBinding binding);

#endif