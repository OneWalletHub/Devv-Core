#!/usr/bin/env python
"""
Tool to create a full Devv shard on localhost.
"""

__copyright__ = "Copyright 2018, Devvio Inc"
__email__ = "security@devv.io"

import os
import tempfile
import argparse
import threading
import subprocess

_validator_bin= 'devv-validator'
_announcer_bin= 'devv-announcer'
_query_bin= 'devv-query'

_endpoint_tmpdir='/tmp/zmqipc'

_endpoint_prefixes = {
    'announcer': {'out': 'announce-out-','in': 'announce-in-'},
    'query': {'out': 'query-'},
    'val0': {'out': 'val0-'},
    'val1': {'out': 'val1-'},
    'val2': {'out': 'val2-'}
}


def create_endpoint(tmpdir, endpoint_prefixes):
    dirs = ['in', 'out']
    endpoints = endpoint_prefixes.copy()
    for pre in endpoint_prefixes:
        for dir in dirs:
            if endpoint_prefixes[pre].get(dir):
                tmp_prefix = os.path.join(tmpdir, endpoint_prefixes[pre][dir])
                tmp = tempfile.NamedTemporaryFile(prefix=tmp_prefix, suffix='.zmqipc')
                endpoints[pre][dir] = 'ipc://' + tmp.name

    return endpoints


def create_announcer(exe, endpoints, configs):
    shard_index = 1
    node_index = 0
    announcer = DevvProcess(exe,
                            shard_index=shard_index,
                            node_index=node_index)
    eps = endpoints['announcer']
    # Add the bind port and pbuf port
    announcer.set_bind_endpoint(eps['out'])
    announcer.set_pbuf_endpoint(eps['in'])
    # Add the config files
    announcer.add_config(configs['devv'])
    announcer.add_config(configs['announcer'])
    announcer.add_config(configs['key-pass'])
    # Set working-dir
    announcer.set_working_dir(configs['working-dir'])
    # Set the key files
    announcer.set_inn_keyfile(os.path.join(configs['base'], 'inn.key'))
    announcer.set_node_keyfile(os.path.join(configs['base'], 'node.key'))

    return announcer


def create_validator(exe, endpoints, configs, hosts, val_id, node_index):
    shard_index = 1
    val = DevvProcess(exe,
                      shard_index=shard_index,
                      node_index=node_index)

    eps = endpoints[val_id]
    # Add the config files
    val.add_config(configs['devv'])
    val.add_config(configs['validator'])
    val.add_config(configs['key-pass'])
    # Set working-dir
    val.set_working_dir(configs['working-dir'])
    # Add the bind port
    val.set_bind_endpoint(eps['out'])
    # add hosts
    for host in hosts:
        if host == val_id:
            # skip if it's us
            #print("{} == {}".format(host, val_id))
            continue
        val.add_host(endpoints[host]['out'])
        #print("{}: add_host({})".format(val_id, host))
    # Set the key files
    val.set_inn_keyfile(os.path.join(configs['base'], 'inn.key'))
    val.set_node_keyfile(os.path.join(configs['base'], 'node.key'))

    return val


def create_query(exe, endpoints, configs, hosts):
    shard_index = 1
    node_index = 0
    query = DevvProcess(exe,
                        shard_index=shard_index,
                        node_index=node_index)
    eps = endpoints['query']
    # Add the pbuf port
    query.set_pbuf_endpoint(eps['out'])
    # Add the config files
    query.add_config(configs['devv'])
    query.add_config(configs['query'])
    query.add_config(configs['key-pass'])
    # Set working-dir
    query.set_working_dir(configs['working-dir'])
    # add hosts
    for val in hosts:
        query.add_host(endpoints[val]['out'])

    return query


class DevvProcess(threading.Thread):
    def __init__(self
                 , exe
                 , shard_index=0
                 , node_index=0
                 , pbuf_endpoint=""
                 , bind_endpoint=""):
        self._exe = exe
        self._shard_index = shard_index
        self._node_index = node_index
        self._pbuf_endpoint = pbuf_endpoint
        self._bind_endpoint = bind_endpoint
        self._num_consensus_threads = 1
        self._num_validator_threads = 1
        self._config_list = []
        self._host_list = []
        self._inn_keyfile = None
        self._node_keyfile = None
        self._working_dir = None

        # Call the base class constructor
        threading.Thread.__init__(self)

    def add_host(self, host):
        self._host_list.append(host)

    def add_config(self, config):
        self._config_list.append(config)

    def set_pbuf_endpoint(self, pbuf_ep):
        self._pbuf_endpoint = pbuf_ep

    def set_bind_endpoint(self, bind_ep):
        self._bind_endpoint = bind_ep

    def set_inn_keyfile(self, keyfile):
        self._inn_keyfile = keyfile

    def set_node_keyfile(self, keyfile):
        self._node_keyfile = keyfile

    def set_working_dir(self, working_dir):
        self._working_dir = working_dir

    def generate_command(self):
        cmd = []
        cmd.append(self._exe)
        cmd.extend(["--shard-index", str(self._shard_index)])
        cmd.extend(["--node-index", str(self._node_index)])
        cmd.extend(["--num-consensus-threads", str(self._num_consensus_threads)])
        cmd.extend(["--num-validator-threads", str(self._num_validator_threads)])

        for config in self._config_list:
            cmd.extend(["--config", config])

        for sub in self._host_list:
            cmd.extend(["--host-list", sub])

        if self._bind_endpoint:
            cmd.extend(["--bind-endpoint", self._bind_endpoint])

        if self._pbuf_endpoint:
            cmd.extend(["--protobuf-endpoint", self._pbuf_endpoint])

        if self._inn_keyfile:
            cmd.extend(["--inn-keys", self._inn_keyfile])

        if self._node_keyfile:
            cmd.extend(["--node-keys", self._node_keyfile])

        if self._working_dir:
            cmd.extend(["--working-dir", self._working_dir])

        return cmd

    def get_config_list(self):
        return self._config_list

    def run(self):
        cmd = self.generate_command()
        logdir = os.path.join(self._working_dir, 'log')
        os.makedirs(logdir, exist_ok=True)
        f = open(os.path.join(logdir, os.path.basename(self._exe)+'-'+str(self._shard_index)+'-'+str(self._node_index)+'.log'), "w+")
        subprocess.run(cmd, timeout=600, stdout=f, stderr=subprocess.STDOUT)
        f.close()


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='devv regression test utility',
                                     epilog="""
Exit status:\n\
  0 if OK
""")

    parser.add_argument('--config-dir', action='store', dest='config_dir',
                        help='Location of the configuration files (devv.conf, devv-*.conf, etc)',
                        default=None, required=True)

    parser.add_argument('--bin-dir', action='store', dest='bin_dir',
                        help='Location of devv-* executables',
                        default=None, required=True)

    parser.add_argument('--working-dir', action='store', dest='working_dir',
                        help='Directory to write log files and chain data files',
                        default=None, required=True)

    args = parser.parse_args()

    validator_exe = os.path.join(args.bin_dir, _validator_bin)
    announcer_exe = os.path.join(args.bin_dir, _announcer_bin)
    query_exe = os.path.join(args.bin_dir, _query_bin)

    os.makedirs(_endpoint_tmpdir, exist_ok=True)
    print("announcer : {}".format(announcer_exe))
    print("validator : {}".format(validator_exe))
    print("query     : {}".format(query_exe))

    config_map = {}
    config_path = args.config_dir
    config_map['base'] = config_path
    config_map['devv'] = os.path.join(config_path, 'devv.conf')
    config_map['announcer'] = os.path.join(config_path, 'devv-announcer.conf')
    config_map['validator'] = os.path.join(config_path, 'devv-validator.conf')
    config_map['query'] = os.path.join(config_path, 'devv-query.conf')
    config_map['key-pass'] = os.path.join(config_path, 'test-key-pass_test.conf')
    config_map['working-dir'] = args.working_dir

    # Create the output director
    os.makedirs(os.path.join(config_map['working-dir'], 'shard-1'), exist_ok=True)

    # Create the ZeroMQ endpoints
    zmq_endpoints = create_endpoint(_endpoint_tmpdir, _endpoint_prefixes)
    for end in zmq_endpoints:
        print("endpoints[{}]: {}".format(end, zmq_endpoints[end]))

    # List of validators
    validators = ['val0', 'val1', 'val2']

    # List of running processes
    devv_processes = []

    #
    # Create the devv-announcer
    #
    announcer = create_announcer(exe=announcer_exe, endpoints=zmq_endpoints, configs=config_map)
    announcer_command = announcer.generate_command()
    print(*announcer_command)
    announcer.start()
    devv_processes.append(announcer)

    #
    # Create devv-query
    query = create_query(exe=query_exe, endpoints=zmq_endpoints, configs=config_map, hosts=validators)
    query_command = query.generate_command()
    print(*query_command)
    query.start()
    devv_processes.append(query)

    # The validators connect to each other and to the announcer
    upstream_hosts = validators + ['announcer']

    # Create the validators
    val_cmds = []
    for i,val in enumerate(validators):
        val_cmds.append(create_validator(exe=validator_exe, endpoints=zmq_endpoints, configs=config_map, hosts=upstream_hosts, node_index=i, val_id=val))

    for v in val_cmds:
        val_command = v.generate_command()
        print(*val_command)
        v.start()
        devv_processes.append(v)


    for proc in devv_processes:
        proc.join()
