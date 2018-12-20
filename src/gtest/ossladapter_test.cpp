/*
 * ossladapter_test.cpp tests the Devv OpenSSL adapter.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "gtest/gtest.h"
#include "common/ossladapter.h"
#include "primitives/Address.h"

namespace {

using namespace Devv;

#define TEST_DESCRIPTION(desc) RecordProperty("OpenSSL interface unit tests", desc)

TEST(ossladapter, VerifyAddressToString_0) {
  std::string str("02957EDD83CCB9AF527F5379FB634835B5A9C265CD7993751E26BAA459C24FBC07");
  LOG_DEBUG << "size: " << str.size() << ":" << kWALLET_ADDR_SIZE;
  std::string addr = str.substr(0, kWALLET_ADDR_SIZE * 2);
  LOG_DEBUG << "addr size: " << addr.size();
  std::vector<byte> addrb(Hex2Bin(addr));
  LOG_DEBUG << "addrb size: " << addrb.size();
  Address a(addrb);
  auto string_out = a.getHexString();
  LOG_INFO << str;
  LOG_INFO << string_out;
  EXPECT_EQ(str, string_out);
}

TEST(ossladapter, LoadPublicKey_0) {
  std::string str("02957EDD83CCB9AF527F5379FB634835B5A9C265CD7993751E26BAA459C24FBC07");
  LOG_DEBUG << "size: " << str.size() << ":" << kWALLET_ADDR_SIZE;
  std::string addr = str.substr(0, kWALLET_ADDR_SIZE * 2);
  LOG_DEBUG << "addr size: " << addr.size();
  std::vector<byte> addrb(Hex2Bin(addr));
  LOG_DEBUG << "addrb size: " << addrb.size();
  Address a(addrb);
  auto key = LoadPublicKey(a);
  EXPECT_NE(key, nullptr);
}

TEST(ossladapter, LoadEcKey_0) {
  std::string str("02514038DA1905561BF9043269B8515C1E7C4E79B011291B4CBED5B18DAECB71E4");
  std::string priv
      ("-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAh8ZjsVLSSMlgICCAAw\nHQYJYIZIAWUDBAECBBBcQGPzeImxYCj108A3DM8CBIGQjDAPienFMoBGwid4R5BL\nwptYiJbxAfG7EtQ0SIXqks8oBIDre0n7wd+nq3NRecDANwSzCdyC3IeCdKx87eEf\nkspgo8cjNlEKUVVg9NR2wbVp5+UClmQH7LCsZB5HAxF4ijHaSDNe5hD6gOZqpXi3\nf5eNexJ2fH+OqKd5T9kytJyoK3MAXFS9ckt5JxRlp6bf\n-----END ENCRYPTED PRIVATE KEY-----");
  std::vector<byte> addr(Hex2Bin(str));
  //std::vector<byte> bytes(str.begin(), str.end());
  Address a(addr);
  auto key = LoadEcKey(str, priv, "password");
  EXPECT_NE(key, nullptr);
}

TEST(ossladapter, GenerateEcKey_0) {
  std::string addr;
  std::string key;
  std::string password("password");
  GenerateEcKey(addr, key, password);
  LOG_DEBUG << "addr: " << addr << " : " << addr.size();
  LOG_DEBUG << "key: " << key << " : " << key.size();
}

TEST(ossladapter, GenerateAndVerify_0) {
  std::string addr;
  std::string key;
  std::string password("password");
  GenerateEcKey(addr, key, password);
  LOG_DEBUG << "addr: " << addr << " : " << addr.size();
  LOG_DEBUG << "key: " << key << " : " << key.size();

  auto eckey = LoadEcKey(addr, key, password);
  EXPECT_NE(eckey, nullptr);
}

TEST(ossladapter, GenerateAndVerify_1) {
  std::string addr;
  std::string key;
  std::string password("password");
  GenerateEcKey(addr, key, password);
  LOG_DEBUG << "addr: " << addr << " : " << addr.size();
  LOG_DEBUG << "key: " << key << " : " << key.size();

  std::vector<byte> addrb(Hex2Bin(addr));
  //std::vector<byte> bytes(str.begin(), str.end());
  Address a(addrb);

  std::string addr_str = a.getHexString();
  auto eckey = LoadEcKey(addr_str, key, password);
  EXPECT_NE(eckey, nullptr);
}


} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
