#!/bin/bash -ex

sleepy=3
devvio_inn=${HOME}/devcash/devvio-inn

echo "Stopping processes"
echo "sleep(${sleepy}) ... 1/4"
sleep ${sleepy}

# Stop the processes
python3 `which shard-control.py` stop

echo "Archiving blockchain"
echo "sleep(${sleepy}) ... 2/4"
sleep ${sleepy}

# Backup and clear blockchain
ssh -i ~/projects/devcash/aws/devv-test-01.pem ec2-user@repeater.shard-1.test.devv.io /mnt/efs/devv-data/query/backup-shard1.sh

echo "Resetting INN DB"
echo "sleep(${sleepy}) ... 3/4"
sleep ${sleepy}

# Reset INN DB
psql -h test-db-01.cdfetsvlzoeo.us-east-2.rds.amazonaws.com -U devv_admin -d database01 < ${devvio_inn}/sql/psql_drop_tables.sql
psql -h test-db-01.cdfetsvlzoeo.us-east-2.rds.amazonaws.com -U devv_admin -d database01 < ${devvio_inn}/sql/psql_dbsetup.sql

echo "Starting processes"
echo "sleep(${sleepy}) ... 4/4"
sleep ${sleepy}

# Restart processes
python3 `which shard-control.py` start
