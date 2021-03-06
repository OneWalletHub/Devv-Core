create or replace function wallet_uuid_by_addr(addr text) returns uuid as $$
declare
  found_wallet uuid;
begin
  found_wallet := '00000000-0000-0000-0000-000000000000'::uuid;
  found_wallet := (SELECT coalesce(wallet_id, found_wallet) from wallet where wallet_addr = upper(addr));
  return found_wallet;
end;
$$ language plpgsql;

create or replace function add_balance(wallet uuid, coin bigint, amount bigint, height int) returns bigint as $$
declare
  new_balance bigint;
begin
  SELECT balance+amount into new_balance from wallet_coin where wallet_id = wallet and coin_id = coin;
  IF FOUND THEN
    UPDATE wallet_coin set balance = new_balance, block_height = height where wallet_id = wallet and coin_id = coin;    
  ELSE
    new_balance := amount;
    BEGIN
      INSERT INTO wallet_coin (wallet_coin_id, wallet_id, block_height, coin_id, balance) (SELECT devv_uuid(), wallet, height, coin, new_balance);
    EXCEPTION WHEN OTHERS THEN
      UPDATE wallet_coin set balance = new_balance, block_height = height where wallet_id = wallet and coin_id = coin;
    END;
  END IF;
  RETURN new_balance;
end;
$$ language plpgsql;

create or replace function handle_coin_request(rx_addr text, shard_id int, block_height int, block_time bigint) returns int as $$
declare
  inn_comment text;
  pending_inn pending_rx%ROWTYPE;
  receiver uuid;
  nil_wallet uuid;
  new_tx_id uuid;
begin
  nil_wallet := '00000000-0000-0000-0000-000000000000'::uuid;
  inn_comment := 'Test Devv from the INN';
  receiver := wallet_uuid_by_addr(rx_addr);
  new_tx_id := (SELECT devv_uuid());
  SELECT * INTO pending_inn from pending_rx where rx_wallet = receiver and comment = inn_comment limit 1;
  IF FOUND THEN
    INSERT INTO tx (tx_id, shard_id, block_height, block_time, tx_wallet, coin_id, amount) (SELECT new_tx_id, shard_id, block_height, block_time, nil_wallet, pending_inn.coin_id, pending_inn.amount);
    INSERT INTO rx (rx_id, shard_id, block_height, block_time, tx_wallet, rx_wallet, coin_id, amount, delay, comment, tx_id) (SELECT devv_uuid(), shard_id, block_height, block_time, nil_wallet, receiver, pending_inn.coin_id, pending_inn.amount, 0, inn_comment, new_tx_id);
    PERFORM add_balance(receiver, pending_inn.coin_id, pending_inn.amount, block_height);
    delete from pending_rx where pending_rx_id = pending_inn.pending_rx_id;
    delete from pending_tx where pending_tx_id = cast(pending_inn.sig as uuid);
    return 1;
  ELSE 
    raise notice 'INN transaction request not found.';
  END IF;
  return 0;
end;
$$ language plpgsql;

create or replace function handle_default_tx(next_sig text, shard int, height int, blocktime bigint) returns int as $$
declare
  pending_tx_row pending_tx%ROWTYPE;
  pending_rx_row pending_rx%ROWTYPE;
begin
  SELECT * INTO pending_tx_row from pending_tx where sig = upper(next_sig);
  IF FOUND THEN
    PERFORM add_balance(pending_tx_row.tx_wallet, pending_tx_row.coin_id, pending_tx_row.amount, height);
    INSERT INTO tx (tx_id, shard_id, block_height, block_time, tx_wallet, coin_id, amount, comment) (SELECT pending_tx_row.pending_tx_id, shard, height, blocktime, pending_tx_row.tx_wallet, pending_tx_row.coin_id, pending_tx_row.amount, pending_tx_row.comment);
    for pending_rx_row in select * from pending_rx where sig = upper(next_sig)
    LOOP
      perform add_balance(pending_rx_row.rx_wallet, pending_rx_row.coin_id, pending_rx_row.amount, height);
      INSERT INTO rx (rx_id, shard_id, block_height, block_time, tx_wallet, rx_wallet, coin_id, amount, delay, comment, tx_id) (SELECT pending_rx_row.pending_rx_id, shard, height, blocktime, pending_tx_row.tx_wallet, pending_rx_row.rx_wallet, pending_rx_row.coin_id, pending_rx_row.amount, 0, pending_rx_row.comment, pending_tx_row.pending_tx_id);
    END LOOP;
    delete from pending_rx where sig = upper(next_sig);
    delete from pending_tx where sig = upper(next_sig);
    return 1;
  ELSE 
    raise notice 'Transaction not initialized.';
  END IF;
  return 0;
end;
$$ language plpgsql;

create or replace function update_for_block(height int) returns int as $$
declare
  update_count int;
  tx RECORD;
begin
  update_count := 0;
  FOR tx IN SELECT * from fresh_tx where block_height = height
  LOOP
    IF tx.oracle_name = 'io.devv.coin_request' THEN
      update_count := handle_coin_request(tx.rx_addr, tx.shard_id, tx.block_height, tx.block_time)+update_count;
    ELSE
      update_count := handle_default_tx(tx.sig, tx.shard_id, tx.block_height, tx.block_time)+update_count;
    END IF;
  END LOOP;
  delete from fresh_tx where block_height = height;
  return update_count;
end;
$$ language plpgsql;

create or replace function reject_old_txs() returns int as $$
declare
  update_count int;
begin
  update_count := (SELECT count(*) from pending_tx where to_reject = true);
  delete from pending_tx where to_reject = true;
  update pending_tx set to_reject = true;
  return update_count;
end;
$$ language plpgsql;
