create tablespace devvdata owner devv_admin location '/devv';
create tablespace devvdex owner devv_admin location '/devvdex';

create or replace function devv_uuid() returns uuid as $$
declare
next_id uuid;
begin
next_id := overlay(overlay(md5(random()::text || ':' || clock_timestamp()::text) placing '4' from 13) placing '8' from 17)::uuid;
return next_id;
end;
$$ language plpgsql;


CREATE SEQUENCE shard_id_seq MINVALUE 0;

-- customer table
CREATE TABLE devvuser (
    devvuser_id uuid constraint pk_devvuserid primary key using index tablespace devvdex,
    devvusername VARCHAR UNIQUE NOT NULL,
    password VARCHAR NOT NULL,
    full_name VARCHAR NOT NULL,
    email VARCHAR NOT NULL,
    phone VARCHAR
) tablespace devvdata;

--account table (customers have accounts)
CREATE TABLE account (
    account_id uuid constraint pk_accountid primary key using index tablespace devvdex,
    devvuser_id uuid references devvuser,
    account_name VARCHAR NOT NULL
) tablespace devvdata;

-- shard table
CREATE TABLE shard (
  shard_id INTEGER NOT NULL DEFAULT nextval('shard_id_seq'::regclass)
      constraint pk_shardid primary key using index tablespace devvdex,
  shard_name VARCHAR UNIQUE NOT NULL
) tablespace devvdata;

-- wallet table (customers have accounts have wallets)
CREATE TABLE wallet (
    wallet_id uuid constraint pk_walletid primary key using index tablespace devvdex,
    wallet_addr text UNIQUE NOT NULL,
    account_id uuid references account,
    shard_id INTEGER NOT NULL references shard,
    wallet_name VARCHAR NOT NULL
) tablespace devvdata;

--currency table (abstract currencies that customers can have)
CREATE TABLE currency (
    coin_id bigint constraint pk_coinid primary key using index tablespace devvdex,
    coin_name VARCHAR UNIQUE NOT NULL
) tablespace devvdata;

--walletCoins table (customers have accounts have wallets have coins)
CREATE TABLE wallet_coin (
    wallet_coin_id uuid constraint pk_walletcoinid primary key using index tablespace devvdex,
    wallet_id uuid NOT NULL references wallet,
    block_height INTEGER NOT NULL,
    coin_id BIGINT NOT NULL references currency,
    balance BIGINT
) tablespace devvdata;

--tx table (debits from customer's account's wallet)
CREATE TABLE tx (
    tx_id uuid constraint pk_txid primary key using index tablespace devvdex,
    shard_id INTEGER NOT NULL references shard,
    block_height INTEGER NOT NULL,
    block_time bigint not null,
    tx_wallet uuid NOT NULL references wallet,
    coin_id BIGINT NOT NULL references currency,
    amount BIGINT,
    comment text
) tablespace devvdata;

--rx table (credits customer's account's wallet)
CREATE TABLE rx (
    rx_id uuid constraint pk_rxid primary key using index tablespace devvdex,
    shard_id INTEGER NOT NULL references shard,
    block_height INTEGER NOT NULL,
    block_time bigint not null,
    tx_wallet uuid NOT NULL references wallet,
    rx_wallet uuid NOT NULL references wallet,
    coin_id BIGINT NOT NULL references currency,
    amount BIGINT,
    delay bigint,
    comment text,
    tx_id uuid not null references tx
) tablespace devvdata;

-- keytable table  for wallets
CREATE TABLE keytable
(
  key_id uuid constraint pk_keytableid primary key using index tablespace devvdex,
  wallet_id uuid NOT NULL references wallet,
  pkey text UNIQUE NOT NULL,
  agent text NOT NULL
) tablespace devvdata;

--pending_tx table (tracks txs not yet finalized in tx)
CREATE TABLE pending_tx (
    pending_tx_id uuid constraint pk_pending_txid primary key using index tablespace devvdex,
    sig text unique not null,
    shard_id INTEGER NOT NULL references shard,
    tx_wallet uuid NOT NULL references wallet,
    coin_id BIGINT NOT NULL references currency,
    amount BIGINT,
    comment text
) tablespace devvdata;

--pending_rx table (tracks txs not yet settled in rx)
CREATE TABLE pending_rx (
    pending_rx_id uuid constraint pk_pending_rxid primary key using index tablespace devvdex,
    sig text not null,
    shard_id INTEGER NOT NULL references shard,
    tx_wallet uuid NOT NULL references wallet,
    rx_wallet uuid NOT NULL references wallet,
    coin_id BIGINT NOT NULL references currency,
    amount BIGINT,
    delay bigint,
    comment text,
    pending_tx_id uuid not null references pending_tx
) tablespace devvdata;

create or replace function reset_state() returns void as $$
begin
truncate table pending_rx cascade;
truncate table pending_tx cascade;
truncate table rx cascade;
truncate table tx cascade;
truncate table wallet_coin cascade;
end;
$$ language plpgsql;

insert into currency (coin_id, coin_name) values (0, 'Devcash');
insert into currency (coin_id, coin_name) values (1, 'Devv');

insert into devvuser (devvuser_id, devvusername, password, full_name, email) values ('00000000-0000-0000-0000-000000000000'::uuid, 'Unknown', 'Unknown', 'Unknown', 'Unknown');
insert into account (account_id, devvuser_id, account_name) values ('00000000-0000-0000-0000-000000000000'::uuid, '00000000-0000-0000-0000-000000000000'::uuid, 'Unknown');
insert into wallet (wallet_id, wallet_addr, account_id, shard_id, wallet_name) values ('00000000-0000-0000-0000-000000000000'::uuid, '00000000-0000-0000-0000-000000000000', '00000000-0000-0000-0000-000000000000'::uuid, 0, 'INN');
insert into shard (shard_name) values ('Shard-0');
insert into shard (shard_name) values ('Shard-1');