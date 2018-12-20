#!/usr/bin/env python
"""
Tool to synchronize Devv validator instances for benchmarking and timing.
"""

__copyright__ = "Copyright 2018, Devvio Inc"
__email__ = "security@devv.io"

import time
import zmq
import sys

if len(sys.argv) < 2:
    print ("Usage {} NUM_NODES".format(sys.argv[0]))
    exit(-1)


num_nodes = int(sys.argv[1])
base_port = int(sys.argv[2])

if num_nodes < 1 or num_nodes > 9:
    print("Num nodes is of range (0 > num_nodex > 10)")
    exit(-1)


context = zmq.Context()
sockets = []

for i in range(num_nodes):
    port = base_port + i
    print("Binding on port {} for node {}".format(port, i))

    socket = context.socket(zmq.REP)
    socket.bind("tcp://*:"+str(port))
    sockets.append(socket)


for i in range(num_nodes):
    print("Waiting for sync message from nodes")
    #  Wait for next request from client
    message = sockets[i].recv()
    print("Received request from: %s" % message)

    #time.sleep(2)


for i in range(num_nodes):
    print("Sending worlds!")
    #  Send reply back to client
    sockets[i].send(b"World")
