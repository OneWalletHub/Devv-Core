#!/bin/bash

node=$1
do_debug=$2

mode="T1"
debug_mode="off"
num_threads=1
proto="tcp"

if [ $do_debug -eq 0 ]; then
    export DEVV_OUTPUT_LOGLEVEL=debug
    num_transactions=1000
    tx_batch_size=100
elif [ $do_debug -eq 1 ]; then
    export DEVV_OUTPUT_LOGLEVEL=warning
    num_transactions=18000
    tx_batch_size=2000
elif [ $do_debug -eq 2 ]; then
    export DEVV_OUTPUT_LOGLEVEL=debug
    num_transactions=36000
    tx_batch_size=3000
else
    export DEVV_OUTPUT_LOGLEVEL=debug
    num_transactions=10000
    tx_batch_size=1000
    gdb="gdb -ex run --args "
fi

trace_file="${HOME}/dmnt/trace/trace_$$_${node}.out"

node0_ip="dc001"
node1_ip="dc002"
node2_ip="dc003"

node0_port="55550"
node1_port="55551"
node2_port="55552"

bind_port[0]="${proto}://*:${node0_port}"
bind_port[1]="${proto}://*:${node1_port}"
bind_port[2]="${proto}://*:${node2_port}"

node0_target="${proto}://${node0_ip}:${node0_port}"
node1_target="${proto}://${node1_ip}:${node1_port}"
node2_target="${proto}://${node2_ip}:${node2_port}"

node_index=(0 1 2)

case $node in
    0)
        echo "setting up node 0"
        hostA[$node]=${node1_target}
        hostB[$node]=${node2_target}
        ;;
    1)
        echo "setting up node 1"
        hostA[$node]=${node0_target}
        hostB[$node]=${node2_target}
        ;;
    2)
        echo "setting up node 2"
        hostA[$node]=${node0_target}
        hostB[$node]=${node1_target}
        ;;
    *)
        echo "Error - unknown node"
        ;;

esac

cmd="${gdb}./devv --node-index ${node} \
--debug-mode ${debug_mode} \
--mode T2 \
--num-consensus-threads ${num_threads} \
--num-validator-threads ${num_threads} \
--host-list ${hostA[$node]} \
--host-list ${hostB[$node]} \
--sync-host devcash \
--output output_${node}.out \
--trace-output ${trace_file} \
--generate-tx ${num_transactions} \
--tx-batch-size ${tx_batch_size} \
--bind-endpoint ${bind_port[$node]}"

echo $cmd
$cmd
