/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class io_devv_test_DevvTestMain */

#ifndef _Included_io_devv_test_DevvTestMain
#define _Included_io_devv_test_DevvTestMain
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     io_devv_test_DevvTestMain
 * Method:    SignTransaction
 * Signature: ([BLjava/lang/String;[B)[B
 */
JNIEXPORT jbyteArray JNICALL Java_io_devv_test_DevvTestMain_SignTransaction
  (JNIEnv *, jobject, jbyteArray, jstring, jbyteArray);

/*
 * Class:     io_devv_test_DevvTestMain
 * Method:    CreateProposal
 * Signature: (Ljava/lang/String;[BLjava/lang/String;Ljava/lang/String;[B)[B
 */
JNIEXPORT jbyteArray JNICALL Java_io_devv_test_DevvTestMain_CreateProposal
  (JNIEnv *, jobject, jstring, jbyteArray, jstring, jstring, jbyteArray);

#ifdef __cplusplus
}
#endif
#endif
