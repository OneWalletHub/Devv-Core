#!/usr/bin/bash -ex

run=$1
shard=$2

env
echo `hostname`

ch_run="/opt/local/bin/ch-run"
image="/z/c-cloud/dirs/x86_64-ubuntu16.04-devcash-v024"
python="python3"
run_shard=${HOME}/dmnt/devcash-core/scripts/run_shard.py
log_dir=${HOME}/run-${run}/shard-${shard}

export DEVV_OUTPUT_LOGLEVEL=info
export PATH=${PATH}:${HOME}/dmnt/devcash-core/build

mkdir -p $log_dir

time timeout 600 $ch_run $image -- $python $run_shard $log_dir
