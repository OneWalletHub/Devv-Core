/*
 * pbuf_test.cpp tests serialization and deserialization
 * of Devv objects in Protobuf format.
 *
 * @copywrite  2018 Devvio Inc
 */
#include <cstdint>

#include "gtest/gtest.h"

#include "primitives/Summary.h"
#include "primitives/Tier1Transaction.h"
#include "primitives/Tier2Transaction.h"
#include "primitives/Transfer.h"
#include "primitives/json_interface.h"
#include "primitives/block_tools.h"

#include "pbuf/devv_pbuf.h"

namespace Devv {
namespace {

#define TEST_DESCRIPTION(desc) RecordProperty("unit tests for pbuf data types and converters", desc)

/**
 *
 * InputBufferTest
 *
 */
class PbufTransactionTest : public ::testing::Test {
 protected:
  PbufTransactionTest()
      : t1_context_0_(0,
                      0,
                      Devv::eAppMode::T1,
                      "", // "../opt/inn.key"
                      "", //"../opt/node.key"
                      "password"), transfers_(), keys_(t1_context_0_) {
    for (int i = 0; i < 4; ++i) {
      keys_.addWalletKeyPair(kADDRs[i], kADDR_KEYs[i], "password");
    }

    keys_.setInnKeyPair(kINN_ADDR, kINN_KEY, "password");

    for (int i = 0; i < 3; ++i) {
      keys_.addNodeKeyPair(kNODE_ADDRs.at(i), kNODE_KEYs.at(i), "password");
    }

    Devv::Transfer transfer1(keys_.getWalletAddr(0), 0, -10, 0);
    transfers_.push_back(transfer1);
    Devv::Transfer transfer2(keys_.getWalletAddr(1), 0, 10, 0);
    transfers_.push_back(transfer2);
  }

  ~PbufTransactionTest() = default;

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
    pb_transaction_0_.set_operation(devv::proto::eOpType::OP_CREATE);
    pb_transaction_0_.set_nonce(Bin2Str(Hex2Bin("86525F0665010000")));
    pb_transaction_0_.set_sig(Bin2Str(Hex2Bin(
        "69306402304BCFD89AE647DACA5A5D64FCEC0F66C66497F03449E8C6EEC239B1F94C1E5FC860CE5C37BBFC3142A4FFF2C857A8E55C023075918475133C249446114AC31D5DBD62AD74C254EDF1C9652D547CE906EF68504390ABC4724ADFB000B1C61454E871CC000000")));

    std::vector<Transfer> transfer_vector;
    auto transfer = Transfer(
        "310272B05D9A8CF6E1565B965A5CCE6FF88ABD0C250BC17AB23745D512095C2AFCDB3640A2CBA7665F0FAADC26B96E8B8A9D",
        0,
        -6,
        0);
    transfer_vector.push_back(transfer);

    transfer = Transfer("2102E14466DC0E5A3E6EBBEAB5DD24ABE950E44EF2BEB509A5FD113460414A6EFAB4", 0, 2, 0);
    transfer_vector.push_back(transfer);

    transfer = Transfer("2102C85725EE136128BC7D9D609C0DD5B7370A8A02AB6F623BBFD504C6C0FF5D9368", 0, 2, 0);
    transfer_vector.push_back(transfer);

    transfer = Transfer("21030DD418BF0527D42251DF0254A139084611DFC6A0417CCE10550857DB7B59A3F6", 0, 2, 0);
    transfer_vector.push_back(transfer);

    for (auto& xfer: transfer_vector) {
      //std::cout << xfer.getJSON() << std::endl;
      auto pb_transfer = pb_transaction_0_.add_xfers();
      pb_transfer->set_address(Bin2Str(xfer.getAddress().getCanonical()));
      pb_transfer->set_coin(xfer.getCoin());
      pb_transfer->set_amount(xfer.getAmount());
      pb_transfer->set_delay(xfer.getDelay());
    }
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  // Create a default context
  Devv::DevvContext t1_context_0_;

  // default Transfer list
  std::vector<Devv::Transfer> transfers_;

  // keys
  Devv::KeyRing keys_;

  devv::proto::Transaction pb_transaction_0_;
};

TEST_F(PbufTransactionTest, defaultConstructor) {
  devv::proto::Transaction pb_tx;

  EXPECT_EQ(pb_tx.operation(), devv::proto::eOpType::OP_CREATE);
}

TEST_F(PbufTransactionTest, createTransfer_0) {
  auto pb_transfer = devv::proto::Transfer();
  std::string
      addr_hex = "310272B05D9A8CF6E1565B965A5CCE6FF88ABD0C250BC17AB23745D512095C2AFCDB3640A2CBA7665F0FAADC26B96E8B8A9D";
  uint64_t coin = 1;
  int64_t amount = -3;
  uint64_t delay = 1;

  pb_transfer.set_address(Bin2Str(Hex2Bin(addr_hex)));
  pb_transfer.set_coin(coin);
  pb_transfer.set_amount(amount);
  pb_transfer.set_delay(delay);

  std::vector<byte> bytes(pb_transfer.address().begin(), pb_transfer.address().end());
  auto address = Address(bytes);
  auto transfer = Transfer(address, pb_transfer.coin(), pb_transfer.amount(), pb_transfer.delay());

  EXPECT_EQ(addr_hex, transfer.getAddress().getJSON());
  EXPECT_EQ(coin, transfer.getCoin());
  EXPECT_EQ(amount, transfer.getAmount());
  EXPECT_EQ(delay, transfer.getDelay());
}

TEST_F(PbufTransactionTest, createTransaction_0) {
  auto pb_transaction = devv::proto::Transaction();
  pb_transaction.set_operation(devv::proto::eOpType::OP_CREATE);
  pb_transaction.set_nonce(Bin2Str(Hex2Bin("86525F0665010000")));
  pb_transaction.set_sig(Bin2Str(Hex2Bin(
      "69306402304BCFD89AE647DACA5A5D64FCEC0F66C66497F03449E8C6EEC239B1F94C1E5FC860CE5C37BBFC3142A4FFF2C857A8E55C023075918475133C249446114AC31D5DBD62AD74C254EDF1C9652D547CE906EF68504390ABC4724ADFB000B1C61454E871CC000000")));

  std::vector<Transfer> transfer_vector;
  auto transfer = Transfer(
      "310272B05D9A8CF6E1565B965A5CCE6FF88ABD0C250BC17AB23745D512095C2AFCDB3640A2CBA7665F0FAADC26B96E8B8A9D",
      0,
      -6,
      0);
  transfer_vector.push_back(transfer);

  transfer = Transfer("2102E14466DC0E5A3E6EBBEAB5DD24ABE950E44EF2BEB509A5FD113460414A6EFAB4", 0, 2, 0);
  transfer_vector.push_back(transfer);

  transfer = Transfer("2102C85725EE136128BC7D9D609C0DD5B7370A8A02AB6F623BBFD504C6C0FF5D9368", 0, 2, 0);
  transfer_vector.push_back(transfer);

  transfer = Transfer("21030DD418BF0527D42251DF0254A139084611DFC6A0417CCE10550857DB7B59A3F6", 0, 2, 0);
  transfer_vector.push_back(transfer);

  for (auto& xfer: transfer_vector) {
    auto pb_transfer = pb_transaction.add_xfers();
    pb_transfer->set_address(Bin2Str(xfer.getAddress().getCanonical()));
    pb_transfer->set_coin(xfer.getCoin());
    pb_transfer->set_amount(xfer.getAmount());
    pb_transfer->set_delay(xfer.getDelay());
  }

  auto transaction = Devv::CreateTransaction(pb_transaction, keys_);

  EXPECT_EQ(transaction->getTransfers().size(), 4);
}

TEST_F(PbufTransactionTest, createTransaction_1) {
  auto nonce_string = Bin2Str(Hex2Bin("86525F0665010000"));
  EXPECT_EQ(pb_transaction_0_.nonce(), nonce_string);
  EXPECT_EQ(pb_transaction_0_.xfers_size(), 4);
}

TEST_F(PbufTransactionTest, createTransaction_2) {
  auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
  auto transaction_1 = Devv::CreateTransaction(pb_transaction_0_, keys_, true);

  EXPECT_EQ(transaction_1->getTransfers().size(), 4);
}

TEST_F(PbufTransactionTest, createTransaction_empty_sig) {
  pb_transaction_0_.set_sig("");
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("Invalid Signature size: 0"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST_F(PbufTransactionTest, createTransaction_sig_wrong_size) {
  pb_transaction_0_.set_sig("abc123");
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("Invalid Signature size: 6"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST_F(PbufTransactionTest, createTransaction_bad_sig) {
  pb_transaction_0_.set_sig(Bin2Str(Hex2Bin(
      "deadbeef000BCFD89AE647DACA5A5D64FCEC0F66C66497F03449E8C6EEC239B1F94C1E5FC860CE5C37BBFC3142A4FFF2C857A8E55C023075918475133C249446114AC31D5DBD62AD74C254EDF1C9652D547CE906EF68504390ABC4724ADFB000B1C61454E871CC000000")));
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("Invalid Signature - size == 106 but byte(0) != 105"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST_F(PbufTransactionTest, createTransaction_big_sig) {
  std::string big_sig(500000000, 'a');
  pb_transaction_0_.set_sig(Bin2Str(Hex2Bin(big_sig)));
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("Invalid Signature size: 250000000"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST_F(PbufTransactionTest, createTransaction_bad_amount) {
  pb_transaction_0_.mutable_xfers(1)->set_amount(100);
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("TransactionError: transaction amounts are asymmetric. (sum=98)"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST_F(PbufTransactionTest, createTransaction_change_delay) {
  pb_transaction_0_.mutable_xfers(1)->set_delay(10);
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("TransactionError: transaction signature did not validate"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST_F(PbufTransactionTest, createTransaction_small_nonce) {
  pb_transaction_0_.set_nonce("1");
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_, true);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("Invalid serialized T2 transaction, nonce is too small (1)"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

TEST_F(PbufTransactionTest, createTransaction_signature_validation) {
  pb_transaction_0_.set_nonce("1");
  try {
    auto transaction_0 = Devv::CreateTransaction(pb_transaction_0_, keys_);
    FAIL() << "Expected std::runtime_error";
  }
  catch (std::runtime_error const& err) {
    EXPECT_EQ(err.what(), std::string("TransactionError: transaction signature did not validate"));
  }
  catch (...) {
    FAIL() << "Expected std::runtime_error";
  }
}

} // unnamed namespace
} // namespace Devv

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
