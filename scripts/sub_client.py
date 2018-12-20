#!/usr/bin/env python
"""
Subscription client to sniff Devv network messages.
"""

__copyright__ = "Copyright 2018, Devvio Inc"
__email__ = "security@devv.io"

import sys
import zmq
import struct

def get_devvmessagetype_string(mess_type):
    if mess_type == 0:
        string = "FINAL_BLOCK"
    elif mess_type == 1:
        string = "PROPOSAL_BLOCK"
    elif mess_type == 2:
        string = "TRANSACTION_ANNOUNCEMENT"
    elif mess_type == 3:
        string = "VALID"
    elif mess_type == 4:
        string = "REQUEST_BLOCK"
    elif mess_type == 5:
        string = "GET_BLOCKS_SINCE"
    elif mess_type == 6:
        string = "BLOCK_SINCE"
    else:
        string = "ERROR_UNKNOWN_TYPE"
    return string

def get_devvmessage(string_buf):
    unpack_str0 = '=1sIii'
    size = struct.calcsize(unpack_str0)
    rem = len(string) - size
    tup = struct.unpack(unpack_str0 + str(rem) + 's', string_buf)
    if ord(tup[0]) != 52:
        return("ERROR - Message header_version != 52")
    #print(tup)
    mess_size = len(tup[4])-tup[3]-4
    tup2 = struct.unpack("="+str(tup[3])+"sI"+str(mess_size)+"s", tup[4])
    if tup2[1] != mess_size:
        return("ERROR - size({}) != message bufsize({})".format(tup2[1], mess_size))
    #print(tup2)
    #print(get_devvmessagetype_string(tup[2]))
    s = "HDR: {}".format(str(ord(tup[0])))
    s += " | IDX: {}".format(tup[1])
    s += " | TPE: {}".format(get_devvmessagetype_string(tup[2]))
    s += " | SZE: {}".format(mess_size)
    return(s)

port = "5556"
if len(sys.argv) > 1:
    port =  sys.argv[1]
    int(port)

if len(sys.argv) > 2:
    port1 =  sys.argv[2]
    int(port1)

# Socket to talk to server
context = zmq.Context()
socket = context.socket(zmq.SUB)

host_str = "tcp://localhost:{}".format(port)
print("Summarizing messages from", host_str)
socket.connect (host_str)

topicfilter = ""
socket.setsockopt_string(zmq.SUBSCRIBE, '')

# Process 5 updates
total_value = 0
for update_nbr in range(500):
    string = socket.recv()
    #print("len: {}".format(len(string)))
    uri = string
    string = socket.recv()
    print("URI: {} | {}".format(uri.decode('utf-8'), get_devvmessage(string)))

print("done.")
