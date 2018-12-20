#!/usr/bin/env python
"""
Usage:
  ./run_shard.py config_file output_dir
Example:
  ./run_shard.py ../../my_config.txt /tmp/output

Creates an X node shard on localhost. Each node will read the config file for
default options. The nodes will be connected to each other and each will also
open a connection to an announcer at "tcp://localhost:(announcer_base_port + node_num).
"""

__copyright__ = "Copyright 2018, Devvio Inc"
__email__ = "security@devv.io"

import sys
import os
import time
import subprocess

nodes=['localhost',
       'localhost',
       'localhost']

announce_node='localhost'

start_processes = True
run_in_charliecloud = False

num_nodes = len(nodes)
num_t2_shards=1
base_port = 56550
announcer_base_port = 57550

config_file = sys.argv[1]
log_dir = sys.argv[2]
pass_file = sys.argv[3]

if num_nodes < 3:
    print("Error, number of nodes must be >= 3")

bind_port = []
node_target = []
output_file = []
for i in range(num_nodes):
    bind_port.append("tcp://*:"+str(base_port+i))
    node_target.append("tcp://"+nodes[i]+":"+str(base_port+i))
    output_file.append(os.path.join(log_dir, "devvnode_output"+str(i)+".log"))

host_list = []
for i in range(num_nodes):
    host_list.append([])
    #node_index=i
    for index, node in enumerate(nodes):
        # skip self
        if (index == i):
            continue
        host_list[i].append(node_target[index])


cmds = []
for i in range(len(nodes)):
    cmds.append([])
    cmd = cmds[i]
    if run_in_charliecloud:
        cmd.append("ch-run")
        cmd.append("/z/c-cloud/dirs/x86_64-ubuntu16.04-devcash-v028")
        cmd.append("--")
    cmd.append("./devcash")
    # set node index
    cmd.extend(["--node-index", str(i)])
    # add config file
    cmd.extend(["--config", config_file])
    # add config file
    cmd.extend(["--config", pass_file])
    # connect to each node
    for host in host_list[i]:
        cmd.extend(["--host-list", host])
    # connect to announcer
    cmd.extend(["--host-list", "tcp://"+announce_node+":"+str(announcer_base_port+i)])
    # bind for incoming connections
    cmd.extend(["--bind-endpoint", bind_port[i]])

#############################

ps = []
for index,cmd in enumerate(cmds):
    print("Node " + str(index) + ":")
    print("   Command: ",*cmd)
    print("   Logfile: ",output_file[index])
    if start_processes:
        with open(output_file[index], "w") as outfile:
            ps.append(subprocess.Popen(cmd, stdout=outfile, stderr=outfile))
            time.sleep(1.5)

if start_processes:
    for p in ps:
        print("Waiting for nodes ... ctl-c to exit.")
        p.wait()

print("Goodbye.")
