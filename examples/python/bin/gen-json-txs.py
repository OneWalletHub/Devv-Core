#!/usr/bin/env python
"""
Python command line interface for common Devv tasks.
"""

__copyright__ = "Copyright 2018, Devvio Inc"
__email__ = "security@devv.io"

import sys
sys.path.append(".")

import argparse
import subprocess
import time
from devv import devv
from google.protobuf.json_format import MessageToJson
import json

sleep_time = 0.001


def get_sig(env, pkeyfile, key_pass, filename=None):
    print('size txs', str(len(env.txs)))

    env_file = None
    if not filename:
        env_file = devv.EnvFile(env, tmp_dir='/tmp')
        filename = env_file.filename()

    print("env filename: ", filename)

    cmd = devv.create_devvsign_command(filename, pkeyfile, key_pass)
    out = subprocess.check_output(cmd)
    print("out: "+str(out))
    sig = out.decode("utf-8").rstrip()

    print("sleeping "+str(sleep_time)+" again")
    time.sleep(sleep_time)

    print("sig: "+sig)

    return sig

def handle_transaction(args):
    print("")
    print("gen-json-txs : {}".format(0.1))
    print("action       : {}".format(args.devv_action))
    print("tx_type      : {}".format(args.operation.upper() if args.operation is None else args.operation))
    print("from         : {}".format(args.source_address))
    print("to           : {}".format(args.dest_address))
    print("amount       : {}".format(args.amount))
    print("coin_id      : {}".format(args.coin_index))
    print("delay        : {}".format(args.transfer_delay))
    print("p_key        : {}".format(args.private_keyfile))
    print("keypass      : {}".format(args.key_pass))
    print("transfer     : {}".format(args.transfer))
    print("num_txes     : {}".format(args.num_transactions))
    print("")

    if (args.source_address):
        if (args.amount < 1):
            raise ValueError("Transfer amount must be greater than zero.")

    tx = devv.DevvTransaction(args.operation, args.nonce)
    if (args.source_address):
        tx.add_transfer(args.source_address, args.coin_index, -args.amount, args.transfer_delay)
        tx.add_transfer(args.dest_address, args.coin_index, args.amount, args.transfer_delay)

    json_envs = []
    for i in range(args.num_transactions):
        env = devv.get_envelope(tx)
        sig = get_sig(env, args.private_keyfile, args.key_pass)
        print("sig: {}", sig)
        env.txs[0].sig = bytes.fromhex(sig)
        print(env)

        json_envs.append(MessageToJson(env))

    if args.to_json:
        with open(args.to_json, 'w') as json_file:
            #json_file.write(json_envs)
            json.dump(json_envs, json_file)

    print("Done")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='devv command-line utility',
                                     epilog="""
Exit status:\n\
  0 if OK
""")

    parser.add_argument('devv_action', action='store',
                        help='Action to perform - create')
    parser.add_argument('--from', action='store', dest='source_address',
                        help='Wallet to transfer coins from (default: %(default)s)',
                        default=None, required=False)
    parser.add_argument('--to', action='store', dest='dest_address',
                        help='Address to transfer coins to (default: %(default)s)',
                        default=None, required=False)
    parser.add_argument('--coin-index', action='store', dest='coin_index', default=1,
                        help='Index of coin type to transfer (default: %(default)s)', type=int)
    parser.add_argument('--transfer-delay', action='store', dest='transfer_delay',
                        help='Transfer delay time in milliseconds (default: %(default)s)',
                        default=0, type=int)
    parser.add_argument('--amount', action='store', dest='amount',
                        help='Amount of coins to transfer (default: %(default)s)', type=int)
    parser.add_argument('--transfer', action='append', dest='transfer',
                        help='A transfer string in the form of "address:coin:amount:delay"')
    parser.add_argument('--nonce', action='store', dest='nonce',
                        help='The string (or hex) representation of a byte array to be stored with this transaction. The parser will attempt to interpret the string as a hexadecimal number; if non-hex characters are used it will fall back to treating the nonce as a string (default: %(default)s)',
                        default="a default nonce string", required=False)
    parser.add_argument('--operation', action='store', dest='operation',
                        help='The type of transaction: CREATE, MODIFY, EXCHANGE, DELETE',
                        required=True)
    parser.add_argument('--private-keyfile', action='store', dest='private_keyfile',
                        help='Location of private key for source address',
                        required=True)
    parser.add_argument('--key-pass', action='store', dest='key_pass',
                        help='NOTE: password for key - TEST ONLY - DO NOT USE REAL PASSWORDS')

    parser.add_argument('--to-json', action='store', dest='to_json',
                        help='Write transaction envelope to JSON file',
                        default=None, required=False)
    parser.add_argument('--num-transactions', action='store', dest='num_transactions',
                        help='Number of transactions to generate', type=int)

    sysgroup = parser.add_argument_group(title='System and networking options')
    sysgroup.add_argument('--tmp-dir', action='store', dest='tmp_dir',
                          help='Directory to hold the temporary transaction protobuf file (default: %(default)s)',
                          default='/tmp')

    args = parser.parse_args()

    handle_transaction(args)
