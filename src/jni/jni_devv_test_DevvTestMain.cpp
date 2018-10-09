/*
 * Google Protobuf based Java JNI integration for Devv.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "jni_devv_test_DevvTestMain.h"
#include "devv.pb.h"
#include "pbuf/devv_pbuf.h"

JNIEXPORT jbyteArray JNICALL Java_jni_devv_test_DevvTestMain_SignTransaction
  (JNIEnv* env, jobject obj, jbyteArray proto_tx
  , jstring password, jbyteArray private_key) {

    std::string pbuf_str;
    jboolean do_copy = true;
    jsize tx_pbuf_len = env->GetArrayLength(proto_tx);
    jbyte* tx_pbuf_body = env->GetByteArrayElements(proto_tx, &do_copy);
    for (int i=0; i<tx_pbuf_len; i++) {
      pbuf_str.push_back(tx_pbuf_body[i]);
    }
    devv::proto::Transaction tx_in;
    tx_in.ParseFromString(pbuf_str);

    const char *pass_ptr= env->GetStringUTFChars(password, &do_copy);
	std::string pass(pass_ptr);

    std::string key_str;
    jsize key_pbuf_len = env->GetArrayLength(private_key);
    jbyte* key_pbuf_body = env->GetByteArrayElements(private_key, &do_copy);
    for (int i=0; i<key_pbuf_len; i++) {
      key_str.push_back(key_pbuf_body[i]);
    }

    Tier2TransactionPtr t2tx_ptr = CreateTransaction(tx_in, key_str, pass);
    devv::proto::Transaction tx_out;
    for (const TransferPtr& xfer_ptr : t2tx_ptr->getTransfers()) {
      devv::proto::Transfer* xfer = tx_out.add_xfers();
      xfer->set_address(Bin2Str(xfer_ptr->getAddress().getCanonical()));
      xfer->set_coin(xfer_ptr->getCoin());
      xfer->set_amount(xfer_ptr->getAmount());
      xfer->set_delay(xfer_ptr->getDelay());
    }
    tx_out.set_nonce(Bin2Str(t2tx_ptr->getNonce()));
    tx_out.set_sig(Bin2Str(t2tx_ptr->getSignature().getCanonical()));
    tx_out.set_operation((devv::proto::eOpType) t2tx_ptr->getOperation());

    size_t final_tx_len = tx_out.ByteSizeLong();
    void* buffer = malloc(final_tx_len);
    tx_out.SerializeToArray(buffer, final_tx_len);

    jbyteArray ret = env->NewByteArray(final_tx_len);
    env->SetByteArrayRegion(ret, 0, final_tx_len, (jbyte*) buffer);
    free(buffer);
    env->ReleaseByteArrayElements(proto_tx, tx_pbuf_body, 0);
    env->ReleaseStringUTFChars(password, pass_ptr);
    env->ReleaseByteArrayElements(private_key, key_pbuf_body, 0);
    return ret;
  }

JNIEXPORT jbyteArray JNICALL Java_jni_devv_test_DevvTestMain_CreateProposal
  (JNIEnv* env, jobject obj, jstring oracle_name, jbyteArray proto_proposal
  , jstring address, jstring password, jbyteArray private_key) {

    std::string pbuf_str;
    jboolean do_copy = true;

    const char *oracle_ptr= env->GetStringUTFChars(oracle_name, &do_copy);
	std::string oracle(oracle_ptr);

    jsize prop_pbuf_len = env->GetArrayLength(proto_proposal);
    jbyte* prop_pbuf_body = env->GetByteArrayElements(proto_proposal, &do_copy);
    for (int i=0; i<prop_pbuf_len; i++) {
      pbuf_str.push_back(prop_pbuf_body[i]);
    }
    devv::proto::Proposal prop_in;
    prop_in.set_oraclename(oracle);
    prop_in.set_data(pbuf_str);

    const char *addr_ptr= env->GetStringUTFChars(address, &do_copy);
	std::string addr(addr_ptr);

    const char *pass_ptr= env->GetStringUTFChars(password, &do_copy);
	std::string pass(pass_ptr);

    std::string key_str;
    jsize key_pbuf_len = env->GetArrayLength(private_key);
    jbyte* key_pbuf_body = env->GetByteArrayElements(private_key, &do_copy);
    for (int i=0; i<key_pbuf_len; i++) {
      key_str.push_back(key_pbuf_body[i]);
    }

    devv::proto::Proposal prop_out;
    prop_out.set_oraclename(oracle);
    prop_out.set_data(pbuf_str);

    size_t final_prop_len = prop_out.ByteSizeLong();
    void* buffer = malloc(final_prop_len);
    prop_out.SerializeToArray(buffer, final_prop_len);

    jbyteArray ret = env->NewByteArray(final_prop_len);
    env->SetByteArrayRegion(ret, 0, final_prop_len, (jbyte*) buffer);
    free(buffer);
    env->ReleaseStringUTFChars(oracle_name, oracle_ptr);
    env->ReleaseByteArrayElements(proto_proposal, prop_pbuf_body, 0);
    env->ReleaseStringUTFChars(address, addr_ptr);
    env->ReleaseStringUTFChars(password, pass_ptr);
    env->ReleaseByteArrayElements(private_key, key_pbuf_body, 0);
    return ret;
  }
