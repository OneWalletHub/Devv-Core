/*
 * ossladapter.h header only C-Style OpenSSL adapter for hashing and ECDSA.
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef SRC_COMMON_OSSLADAPTER_H_
#define SRC_COMMON_OSSLADAPTER_H_

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "common/binary_converters.h"
#include "common/util.h"
#include "primitives/Address.h"
#include "primitives/Signature.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
inline void EVP_MD_CTX_free(EVP_MD_CTX *ctx)
{
  EVP_MD_CTX_cleanup(ctx);
  OPENSSL_free(ctx);
}
#endif

/** Gets the EC_GROUP for normal transactions.
 *  @return a pointer to the EC_GROUP
 */
static EC_GROUP* GetWalletGroup() {
  return(EC_GROUP_new_by_curve_name(NID_secp256k1));
}

static EC_KEY* GetWalletKey() {
  return(EC_KEY_new_by_curve_name(NID_secp256k1));
}

/** Gets the EC_GROUP for internal transactions.
 *  @return a pointer to the EC_GROUP
 */
static EC_GROUP* GetNodeGroup() {
  return(EC_GROUP_new_by_curve_name(NID_secp384r1));
}

static EC_KEY* GetNodeKey() {
  return(EC_KEY_new_by_curve_name(NID_secp384r1));
}

/** Generates a new EC_KEY.
 *  @pre an OpenSSL context must have been initialized
 *  @param publicKey [out] reference with ASCII armor
 *  @param pkHex [out] references private key with ASCII armor, pem, aes
 *  @param aes_password [in] the password for aes
 *  @return if success, a pointer to the EC_KEY object
 *  @return if error, a NULLPTR
 */
static EC_KEY* GenerateEcKey(std::string& publicKey, std::string& pk
                           , const std::string& aes_password) {
  CASH_TRY {
    EC_GROUP* ecGroup = GetWalletGroup();
    if (NULL == ecGroup) {
      LOG_ERROR << "Failed to generate EC group.";
    }

    EC_KEY *eckey=EC_KEY_new();
    if (NULL == eckey) {
      LOG_ERROR << "Failed to allocate EC key.";
    }

    EC_GROUP_set_asn1_flag(ecGroup, OPENSSL_EC_NAMED_CURVE);
    int state = EC_KEY_set_group(eckey, ecGroup);
    if (1 != state) { LOG_ERROR << "Failed to set EC group status."; }
    state = EC_KEY_generate_key(eckey);
    if (1 != state) { LOG_ERROR << "Failed to generate EC key."; }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    OpenSSL_add_all_algorithms();
#endif
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!EVP_PKEY_set1_EC_KEY(pkey, eckey)) {
      LOG_ERROR << "Could not export private key";
    }

    if (1 != EC_GROUP_check(EC_KEY_get0_group(eckey), NULL)) {
      CASH_THROW("EC group is invalid!");
    }

    if (1 != EC_KEY_check_key(eckey)) {
      LOG_ERROR << "key is invalid!";
    }

    const EVP_CIPHER* cipher = EVP_aes_128_cbc();
    BIO* fOut = BIO_new(BIO_s_mem());
    int result = PEM_write_bio_PKCS8PrivateKey(fOut, pkey, cipher, NULL, 0, NULL,
      const_cast<char*>(aes_password.c_str()));
    if (result != 1) { LOG_ERROR << "Failed to generate PEM private key file"; }
    char buffer[1024];
    memset(buffer, 0, 1024);
    while (BIO_read(fOut, buffer, 1024) > 0) {
      pk += buffer;
      memset(buffer, 0, 1024);
    }
    BIO_free(fOut);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_cleanup();
#endif
    const EC_POINT *pubKey = EC_KEY_get0_public_key(eckey);
    publicKey = EC_POINT_point2hex(ecGroup, pubKey, POINT_CONVERSION_COMPRESSED, NULL);

    return eckey;
  } CASH_CATCH(const std::exception& e) {
    LOG_WARNING << Devv::FormatException(&e, "Crypto");
  }
  return nullptr;
}

/** Generates new EC_KEYs and writes the key pairs to a file.
 *  @pre an OpenSSL context must have been initialized
 *  @param path [in] the path of the file to write
 *  @param num_keys [in] the number of key pairs to generate
 *  @param aes_password [in] the password for aes
 *  @return if success, true (file with key pairs should exist)
 *  @return if error, false
 */
static bool GenerateAndWriteKeyfile(const std::string& path, size_t num_keys
                                    , const std::string& aes_password) {
  std::string output;
  output.reserve(num_keys*(Devv::kFILE_KEY_SIZE+(Devv::kWALLET_ADDR_BUF_SIZE*2)));
  for (size_t i=0; i<num_keys; ++i) {
    std::string addr;
    std::string key;
    GenerateEcKey(addr, key, aes_password);
    output += addr+"\n"+key;
  }
  std::ofstream out_file(path);
  if (out_file.is_open()) {
    out_file << output;
    out_file.close();
  } else {
    LOG_WARNING << "Failed write keys to file '" << path << "'.";
    return false;
  }
  return true;
}

/** Checks an existing EC_KEY using openssl functions.
 *  @pre an OpenSSL context must have been initialized
 *  @param key [in] pointer to the EC key
 *  @throw if error, std::runtime_error
 *  @return true if success, this is a completely functional key
 *  @return false if pointer key only has a public key
 */
static bool ValidateKey(EC_KEY* key) {
  if (1 != EC_GROUP_check(EC_KEY_get0_group(key), nullptr)) {
	LOG_ERROR << "group is invalid!";
  }

  int ret = EC_KEY_check_key(key);
  LOG_DEBUG << "OpenSSL version: " << OPENSSL_VERSION_NUMBER;
  if (ret != 1) {
	auto ossl_ver = OPENSSL_VERSION_NUMBER;
	auto err = ERR_get_error();
	/*
	 * EC_KEY_check_key() will fail here because ec_key does
	 * not have a valid private key. The magical numbers below
	 * represent the OpenSSL version and the private key error
	 * code. The error code is version dependent. We check the err or
	 * code and only throw if it not a private key error.
	 *
	 * if ((ossl_ver == 1.0.2) and (err == invalid private key) or
	 *     (ossl_ver >= 1.1.0) and (err == invalid private key))
	 */
	if ((ossl_ver == 268443775 && err == 269160571) ||
		(ossl_ver >= 269484159 && err == 269492347)) {
	  // error because private key is not set - ignore
	  return false;
	} else {
	  std::string err_str(ERR_error_string(err, NULL));
	  LOG_ERROR << "key is invalid: " << err_str << " : errno " << err;
	  throw std::runtime_error("key is invalid: " + err_str);
	}
  }
  return true;
}

/** Loads an existing EC_KEY based on the provided key pair.
 *  @pre an OpenSSL context must have been initialized
 *  @param publicKey [in] reference compressed as hex
 *  @param privKey [in] references private key with ASCII armor, pem, aes
 *  @param aes_password [in] the password for aes
 *  @throw if error, std::runtime_error
 *  @return if success, a pointer to the EC_KEY object
 *  @return if error, a NULLPTR
 */
static EC_KEY* LoadEcKey(const std::string& publicKey
                       , const std::string& privKey
                       , const std::string& aes_password) {
  if (aes_password.size() < 1) {
    throw std::runtime_error("aes_password cannot be zero-length");
  }
    EC_GROUP* ecGroup;
    if (publicKey.size() == Devv::kWALLET_ADDR_SIZE*2) {
      ecGroup = GetWalletGroup();
    } else if (publicKey.size() == Devv::kNODE_ADDR_SIZE*2) {
      ecGroup = GetNodeGroup();
    } else {
      throw std::runtime_error("Invalid public key!");
    }
    if (NULL == ecGroup) {
      throw std::runtime_error("Failed to generate EC group.");
    }

    EC_KEY *eckey = EC_KEY_new();
    if (NULL == eckey) {
      LOG_ERROR << "Failed to allocate EC key.";
    }

    EC_GROUP_set_asn1_flag(ecGroup, OPENSSL_EC_NAMED_CURVE);
    if (1 != EC_KEY_set_group(eckey, ecGroup)) {
      LOG_ERROR << "Failed to set EC group status.";
    }

    EC_POINT* tempPoint = NULL;
    const char* pubKeyBuffer = &publicKey[0u];
    const EC_POINT* pubKeyPoint = EC_POINT_hex2point(EC_KEY_get0_group(eckey), pubKeyBuffer, tempPoint, NULL);
    if (eckey == NULL) { LOG_ERROR << "Invalid public key point."; }

    OpenSSL_add_all_algorithms();
    EVP_PKEY* pkey = EVP_PKEY_new();
    char buffer[privKey.size()+1];
    strcpy(buffer, privKey.c_str());
    BIO* fIn = BIO_new(BIO_s_mem());
    BIO_write(fIn, buffer, privKey.size()+1);
    pkey = PEM_read_bio_PrivateKey(fIn, NULL, NULL
           , const_cast<char*>(aes_password.c_str()));
    if (pkey == nullptr) {
      std::string err(ERR_error_string(ERR_get_error(),NULL));
      throw std::runtime_error("PEM_read_bio_PrivateKey returned nullptr: " + err);
    }
    eckey = EVP_PKEY_get1_EC_KEY(pkey);
    if (eckey == nullptr) {
      throw std::runtime_error("PEM_rPKEY_get_EC_KEY returned nullptr");
    }
    EC_KEY_set_public_key(eckey, pubKeyPoint);

    if (!ValidateKey(eckey)) {
      LOG_WARNING << "LoadEcKey() returns without valid private key.";
    }
    return(eckey);
}

/** Loads an existing public EC_KEY based on the provided public key.
 *  @pre an OpenSSL context must have been initialized
 *  @param public_key [in] reference compressed as Address
 *  @throw if error, std::runtime_error
 *  @return if success, a pointer to the EC_KEY object
 *  @return if error, a NULLPTR
 */
static EC_KEY* LoadPublicKey(const Devv::Address& public_key) {
    EC_GROUP* ec_group;
    EC_KEY* ec_key;

    if (public_key.isWalletAddress()) {
      ec_group = GetWalletGroup();
      ec_key = GetWalletKey();
    } else if (public_key.isNodeAddress()) {
      ec_group = GetNodeGroup();
      ec_key = GetNodeKey();
    } else {
      throw std::runtime_error("Invalid public key!");
    }
    if (ec_group == nullptr) {
      throw std::runtime_error("Failed to generate EC group.");
    }

    if (ec_key == nullptr) {
      LOG_ERROR << "Failed to allocate EC key.";
    }

    int ret = EC_KEY_set_group(ec_key, ec_group);
    if (ret != 1) {
      LOG_ERROR << "Failed to set EC group status.";
    }

    ret = EC_KEY_generate_key(ec_key);
    if (ret != 1) { LOG_ERROR << "Failed to generate EC key."; }

    std::string publicKey = public_key.getHexString();

    EC_POINT* tempPoint = EC_POINT_new(ec_group);
    const char* pubKeyBuffer = &publicKey[0u];
    EC_POINT_hex2point(EC_KEY_get0_group(ec_key), pubKeyBuffer, tempPoint, NULL);

    if (ec_key == nullptr) { LOG_ERROR << "Invalid public key point."; }

    ret = EC_KEY_set_public_key(ec_key, tempPoint);

    if (ret != 1) {
      LOG_ERROR << "set_public_key failed";
      std::string err(ERR_error_string(ERR_get_error(),NULL));
      EC_POINT_free(tempPoint);
      EC_GROUP_free(ec_group);
      EC_KEY_free(ec_key);
      throw std::runtime_error("set_public_key failed:"+err);
    }

    EC_POINT_free(tempPoint); // good results
    EC_GROUP_free(ec_group); // good results

    return(ec_key);
}

/** Hashes a bytestring with SHA-256.
 *  @note none of the parameters of this function are const.
 *  Parameters may be altered by this function.
 *  @param[in] msg the bytestring to hash
 *  @return the calculated hash
 */
static Devv::Hash DevvHash(const std::vector<Devv::byte>& msg) {
  Devv::Hash md;
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, &msg[0], msg.size());
  SHA256_Final(&md[0], &sha256);
  return md;
}

/** Verifies the signature corresponding to a given string
 *  @param ecKey pointer to the public key that will check this signature
 *  @param msg pointer to the message digest to verify
 *  @param msg_size the size of the message
 *  @param sig pointer to the binary signature to verify
 *  @param sig_size size of the signature, should be 72 bytes
 *  @return true iff the signature verifies with the provided key and context
 *  @return false otherwise
 */
static bool VerifyByteSig(EC_KEY* ecKey, const Devv::Hash& msg
    , const Devv::Signature& sig) {
  try {
    EVP_MD_CTX *ctx;
    if(!(ctx = EVP_MD_CTX_create())) {
      LOG_FATAL << "Could not create signature context!";
    }
    Devv::Hash temp = msg;
    std::vector<unsigned char> raw_sig(sig.getRawSignature());
    std::vector<unsigned char> copy_sig(raw_sig.size()+1);
    for (size_t i=0; i<raw_sig.size(); ++i) {
      copy_sig[i] = raw_sig[i];
    }
    auto p = &copy_sig[0];

    ECDSA_SIG *signature = d2i_ECDSA_SIG(NULL,
                                         const_cast<const unsigned char**>(&p),
					 raw_sig.size());

    int state = ECDSA_do_verify((const unsigned char*) &temp[0],
                                SHA256_DIGEST_LENGTH,
                                signature,
                                ecKey);
    ECDSA_SIG_free(signature);
    EVP_MD_CTX_free(ctx);
    return(1 == state);
  } catch (const std::exception& e) {
    LOG_WARNING << Devv::FormatException(&e, "Crypto.verifySignature");
  }
  return(false);
}

/**
 * Generates the signature for a given string and key pair
 * @pre the EC_KEY must include a private key
 * @pre the OpenSSL context must be intialized
 * @param ec_key pointer to the public key that will check this signature
 * @param msg pointer to the message digest to sign
 * @return Resulting signature
 */
static Devv::Signature SignBinary(EC_KEY* ec_key, const Devv::Hash& msg) {
  if (ec_key == nullptr) {
    throw std::runtime_error("ec_key == nullptr");
  }
  try {
    Devv::Hash temp = msg;
    ECDSA_SIG *signature = ECDSA_do_sign((const unsigned char*) &temp[0],
        SHA256_DIGEST_LENGTH, ec_key);
    int state = ECDSA_do_verify((const unsigned char*) &temp[0],
        SHA256_DIGEST_LENGTH, signature, ec_key);

    //0 -> invalid, -1 -> openssl error
    if (1 != state) {
      LOG_ERROR << "Signature did not validate("+std::to_string(state)+")";
    }

    int len = i2d_ECDSA_SIG(signature, NULL);
    if (len < 80) { //must be 256-bit
      std::array<unsigned char, Devv::kWALLET_SIG_SIZE> a;
      unsigned char* ptr = &a[0];
      memset(&a[0], 6, len);
      len = i2d_ECDSA_SIG(signature, &ptr);
      std::vector<unsigned char> vec_sig(std::begin(a),std::end(a));
      Devv::Signature sig(vec_sig);
      ECDSA_SIG_free(signature);
      return sig;
    } else { //otherwise 384-bit
      std::array<unsigned char, Devv::kNODE_SIG_SIZE> a;
      unsigned char* ptr = &a[0];
      memset(&a[0], 6, len);
      len = i2d_ECDSA_SIG(signature, &ptr);
      std::vector<unsigned char> vec_sig(std::begin(a),std::end(a));
      Devv::Signature sig(vec_sig);
      ECDSA_SIG_free(signature);
      return sig;
    }
  } catch (const std::exception& e) {
    LOG_WARNING << Devv::FormatException(&e, "Crypto.sign");
  }
  throw std::runtime_error("Signature procedure failed!");
}

#endif /* SRC_COMMON_OSSLADAPTER_H_ */
