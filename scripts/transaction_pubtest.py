#!/usr/bin/env python
"""
Tool to publish a test Transaction to a Devv validator.
"""

__copyright__ = "Copyright 2018, Devvio Inc"
__email__ = "security@devv.io"

import sys
sys.path.append(".")

import zmq
import time
import devv_pb2

tx = devv_pb2.Transaction()

tsfrs = []

tsfr = devv_pb2.Transfer()
tsfr.address = bytes.fromhex("310272B05D9A8CF6E1565B965A5CCE6FF88ABD0C250BC17AB23745D512095C2AFCDB3640A2CBA7665F0FAADC26B96E8B8A9D")
tsfr.coin = 0
tsfr.amount = -6
tsfr.delay  = 0
tsfrs.append(tsfr)

tsfr = devv_pb2.Transfer()
tsfr.address = bytes.fromhex("2102E14466DC0E5A3E6EBBEAB5DD24ABE950E44EF2BEB509A5FD113460414A6EFAB4")
tsfr.coin = 0
tsfr.amount = 2
tsfr.delay  = 0
tsfrs.append(tsfr)

tsfr = devv_pb2.Transfer()
tsfr.address = bytes.fromhex("2102C85725EE136128BC7D9D609C0DD5B7370A8A02AB6F623BBFD504C6C0FF5D9368")
tsfr.coin = 0
tsfr.amount = 2
tsfr.delay  = 0
tsfrs.append(tsfr)

tsfr = devv_pb2.Transfer()
tsfr.address = bytes.fromhex("21030DD418BF0527D42251DF0254A139084611DFC6A0417CCE10550857DB7B59A3F6")
tsfr.coin = 0
tsfr.amount = 2
tsfr.delay  = 0
tsfrs.append(tsfr)

tx.xfers.extend(tsfrs)
tx.operation = devv_pb2.OP_CREATE
tx.nonce = bytes.fromhex("86525F0665010000")
tx.sig = bytes.fromhex("69306402304BCFD89AE647DACA5A5D64FCEC0F66C66497F03449E8C6EEC239B1F94C1E5FC860CE5C37BBFC3142A4FFF2C857A8E55C023075918475133C249446114AC31D5DBD62AD74C254EDF1C9652D547CE906EF68504390ABC4724ADFB000B1C61454E871CC000000")

context = zmq.Context()
socket = context.socket(zmq.REQ)
socket.connect("tcp://localhost:55706")

print("Sending message in 3")
time.sleep(3)

env = devv_pb2.Envelope()
env.txs.extend([tx])

socket.send(env.SerializeToString())

msg = socket.recv()

print("response: " + msg.decode("utf-8"))

'''
with open("transaction.pbuf", "wb") as f:
    f.write(env.SerializeToString())
    f.close()
'''

print("Done")

time.sleep(1)
