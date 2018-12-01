/*
 * devv_constants.h
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <cstdlib>
#include <string>

#include "common/devv_types.h"

namespace Devv {

static const unsigned int kDEFAULT_WORKERS = 8;

// millis of sleep between main shut down checks
static const int kMAIN_WAIT_INTERVAL = 100;

static const size_t kWALLET_ADDR_SIZE = 33;
static const size_t kNODE_ADDR_SIZE = 49;

static const size_t kWALLET_ADDR_BUF_SIZE = kWALLET_ADDR_SIZE + 1;
static const size_t kNODE_ADDR_BUF_SIZE = kNODE_ADDR_SIZE + 1;

static const size_t kFILE_KEY_SIZE = 379;
static const size_t kFILE_NODEKEY_SIZE = 448;

static const uint64_t kDUPLICATE_HORIZON_MILLIS = 1000*60*60*24;
static const uint64_t kPROPOSAL_EXPIRATION_MILLIS = 2000;

//Transaction constants

/**
 * Types of operations performed by transactions
 */
enum eOpType : byte { Create = 0, Modify = 1, Exchange = 2, Delete = 3, NumOperations = 4 };

static const std::string kVERSION_TAG = "v";
static const std::string kPREV_HASH_TAG = "prev";
static const std::string kMERKLE_TAG = "merkle";
static const std::string kBYTES_TAG = "bytes";
static const std::string kTIME_TAG = "time";
static const std::string kTX_SIZE_TAG = "txlen";
static const std::string kVAL_COUNT_TAG = "vcount";
static const std::string kTXS_TAG = "txs";
static const std::string kSUM_TAG = "sum";
static const std::string kVAL_TAG = "vals";
static const std::string kXFER_SIZE_TAG = "xfer_size";
static const std::string kSUMMARY_TAG = "summary";
static const std::string kSUM_SIZE_TAG = "sum_size";
static const std::string kOPER_TAG = "oper";
static const std::string kXFER_TAG = "xfer";
static const std::string kNONCE_TAG = "nonce";
static const std::string kNONCE_SIZE_TAG = "nonce_size";
static const std::string kSIG_TAG = "sig";
static const std::string kVALIDATOR_DEX_TAG = "val_dex";

static const size_t kTX_MIN_SIZE = (89 + 2);
static const size_t kENVELOPE_SIZE = 17;
static const size_t kOPERATION_OFFSET = 16;
static const size_t kTRANSFER_OFFSET = 17;
static const size_t kMIN_NONCE_SIZE = 8;
static const size_t kUINT64_SIZE = 8;
static const size_t kTRANSFER_NONADDR_DATA_SIZE = kUINT64_SIZE*3;

static const std::string kINN_KEY = "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIIBDjBJBgkqhkiG9w0BBQ0wPDAbBgkqhkiG9w0BBQwwDgQIGT9wNeMNNvcCAggAMB0GCWCGSAFlAwQBAgQQnleRZC6seC/5+gF5W91OTwSBwAjNeW4dkUbohXhuuy8BGlx7wV/AtBQeMwrVCG/eXj8H9mG/P2MfPf1kbYa18qFOiw46Ih7HhP9QaFVBoNHtsv8/epwAw5stFKIFO/AIDu0piqBqy9KpAEVKTbU2uTqCgGZKRnHCAmE9jgnJoboE41ySWmr7P63tq4LVNIwtPI0xSHxn9fBe1i7BaIQV3fUhGlPW7bgMODoVe7tysCldM/l1O1iDX/eodHOTv0RQjraNeZVqzsPBrSOv7jAgjS0AGA==\n-----END ENCRYPTED PRIVATE KEY-----";

static const std::string kINN_ADDR = "0272B05D9A8CF6E1565B965A5CCE6FF88ABD0C250BC17AB23745D512095C2AFCDB3640A2CBA7665F0FAADC26B96E8B8A9D";

static const std::vector<std::string> kADDRs = {
      "02514038DA1905561BF9043269B8515C1E7C4E79B011291B4CBED5B18DAECB71E4",
      "035C0841F8F62271F3058F37B32193360322BBF0C4E85E00F07BCB10492E91A2BD",
      "02AF4E6872BECE20FA31F181E891D60C7CE49F95D5EF9448F8B7F7E8D621AE008C",
      "03B17F124E174A4055128B907401CAEECF6D5BC56A7DF6201381A8B939CAC12721",
      "025054C2E2C8878B5F0573332CD6556BEB3962E723E35C77582125D766DCBDA567",
      "022977B063FF1AA08758B0F5CBDA720CBC055DFE991741C4F01F110ECAB5789D03"
  };

static const std::vector<std::string> kADDR_KEYs = {
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAh8ZjsVLSSMlgICCAAw\nHQYJYIZIAWUDBAECBBBcQGPzeImxYCj108A3DM8CBIGQjDAPienFMoBGwid4R5BL\nwptYiJbxAfG7EtQ0SIXqks8oBIDre0n7wd+nq3NRecDANwSzCdyC3IeCdKx87eEf\nkspgo8cjNlEKUVVg9NR2wbVp5+UClmQH7LCsZB5HAxF4ijHaSDNe5hD6gOZqpXi3\nf5eNexJ2fH+OqKd5T9kytJyoK3MAXFS9ckt5JxRlp6bf\n-----END ENCRYPTED PRIVATE KEY-----",
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAj26KaMMBcx8gICCAAw\nHQYJYIZIAWUDBAECBBCAi3UqcRVw4celU/EfTYlwBIGQNWD5TjggnHK2yqDOq9uh\nRhQ9ZYWecLKFzirki+/nLdlk+MBIGQIZ/Ra4LStBc6pu0hv6HAEzjt9hhOAh2dLG\n28kieREpPY1vvNpfv3w9cQ5aH0iFoQXCxjwluGOpUBEmc212U56XivOE5DGchx81\nKxLIpWF4c8Jb3dwBdL4GuKCbt+x7AK+Hjj6XnUAhiNGN\n-----END ENCRYPTED PRIVATE KEY-----",
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAhzjMEUwJByZwICCAAw\nHQYJYIZIAWUDBAECBBAFlRRNbCc7mjdARZH4AgWKBIGQ9plIbYr2WbEMY4N/bV+j\n+uI0k0hn68jzZjmzDIesAjRsqJ6EsKnwDb93fW85iA/wF44K4fgn8Y8fAfAUymdQ\nMtXnSbSuteOGfuYvVnusL211UQkg8Al34pTB1igTkEWh3BpTp2/Qf1CpLIFiOeMq\nNcMaALuG2w4gkJaHS33AS2GvHCr6FA8Q7rKNrFNt3HzB\n-----END ENCRYPTED PRIVATE KEY-----",
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAihcpS7r/V0BAICCAAw\nHQYJYIZIAWUDBAECBBACED7jSSDz9pnEViP6WpN/BIGQUQHkt+twjgpGr8LRWQuW\niAiFT7dWMGUIAS1esRkJytVPwQDuYiSf406kqRHWoH5ZeWK0Kk2VvEoVuPlvA1nW\nJhYRnlA1R2IL3I4OftT6CORBF1i8EBM3Ah+bUbcqh4VJUw+weABLXq/0+k1tVA8e\nsxqcUZVQk/n7g6kuoeZgi8gSCrEXEby67MTnkvjlfkpy\n-----END ENCRYPTED PRIVATE KEY-----",
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAh1BrpIaOV33AICCAAw\nHQYJYIZIAWUDBAECBBDJH6sU+1WQnLPNGK5Dwz1DBIGQg08MOyvhVV3UohqZ5Agy\nNNX7TMdmWC1rel35ur6GWM77Cl4uZvT4P56hinbrrtH4En5VUQTF70JVaCyzf5X9\nqGXKxOyFez4mWEK9gQWsYfFQSsjU+DW6Z6wb7pscwFQ4zL4o2+61M8PTTMm6WoyD\nd+9aeJvaUSIvYTWFkiHPuz52Uhyguyp+qcad/yokx7eI\n-----END ENCRYPTED PRIVATE KEY-----",
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAg/scbUqllDxgICCAAw\nHQYJYIZIAWUDBAECBBAPtIhz2lR4u4SoxGiwuEJnBIGQr0cLlguY/h/juBrESEq4\nk6TuOSyqg64tr7E/wRclcIP3CHpnQBvlyd2M3Rq2WCGOObhrJ7MvvoSk4Bm/evWO\nbgDW7HqC84OdNr49nJe/+1RyS8OglA+TFQi0HEQIHn+ePtygDpIdeyKGdXZkqbaN\npbioudfHPQ5Lv2CsEBmSvR6Eb/DyxOuz7JZtoSd55T3F\n-----END ENCRYPTED PRIVATE KEY-----"
  };

static const std::vector<std::string> kNODE_KEYs = {
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIIBDjBJBgkqhkiG9w0BBQ0wPDAbBgkqhkiG9w0BBQwwDgQIZjcCo0ODrskCAggAMB0GCWCGSAFlAwQBAgQQN/J3yTpQ7unPZGT9DKJlmgSBwLXr2Pp4GSwXacelxUk3KFWVUYfcsQGz3JYO3GTdoSyLuWjsRpRA82PSQaP1Vn5C48ZZJlXAwxRn547eBu5WZ2QMGiAwNIzmFKqYI9pYiWsM13F82uDmFGkjbt2HSRx6tIhuA9A/40PIqjVjdkjQkEdiFxh/cjytd4kovH9vee17nSQ29m39AJdFAbukWSak2C5VKvysn1BItZE3qzZVITIV4IBEGRiOjXZcacXKxdu3rjsF9XkD/dV1x/qMLTuycA==\n-----END ENCRYPTED PRIVATE KEY-----",
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIIBDjBJBgkqhkiG9w0BBQ0wPDAbBgkqhkiG9w0BBQwwDgQIb3S5HoFxfvYCAggAMB0GCWCGSAFlAwQBAgQQr21QQhxXn9VGTgyehabg3QSBwE6NAVP/zFWXKUVVE0daHgj81vgfiOJAvi1ayQPzaGv26alNAIu22ntLK+SWcKrqEIrdqCzOT3SoIs8XtEUibpj14ZOp7UzhGQnPsme7oFX2caCVQh76pNfPzJkMoEH2YX/m0HyNBrsdBkWORiPRdfgiv/gMkBIN8ai2jdmWzBmlOX+coQ3hMG5p3rSoQPxBm5vPONQIaM9DlZrwkMq6fXCz0HZwdCLiqKJs0Xl1V4WCXmtXslxeyTPZq/c2YucVPg==\n-----END ENCRYPTED PRIVATE KEY-----",
      "-----BEGIN ENCRYPTED PRIVATE KEY-----\nMIIBDjBJBgkqhkiG9w0BBQ0wPDAbBgkqhkiG9w0BBQwwDgQIC3emoeSyM8MCAggAMB0GCWCGSAFlAwQBAgQQ8DCivDaU4M6hX2WLrHN41ASBwCtuSZ/ybssOKRxe3a72EcqhKvdpWaT/x6AGIOzdakpAnX24YOEadUhgoRlZjHmQ0jMKezOXgcoaOJ9y7+zxyfoBJZnKAKiJhn4Jz5pdQW17riHucsVqOvHHhO+3UtwzpPrrulcjZRn29AwN6HeuvjvZRxbn16Isst97Sal1EADNw5Hri/MJ5BHQWe6LRJbTKv3llP6mAgjuwyZECInjBMZLjfFZLtF1lrviHCFR9ZA/QLoQti4fjdzz1Lurpmf0zw==\n-----END ENCRYPTED PRIVATE KEY-----"
  };

static const std::vector<std::string> kNODE_ADDRs = {
      "03EFE5DA5D03078E91FFF5FA5F786190F53F77D7CBA3FFB7DA1F11D50222403DBD9ED299C91ABC5C0AC4367BF260A4083E",
      "0286C1B1A066728FAABA4D9C795846FF9910C44BD54386F05E088F409BFE56906D41AD60FD1C982920C9017A799E7F9DFF",
      "02D6663B8C28C80D977C8943B9D9C6441E1C2D08FBA43131016CCCB11B358B531A3379E1289206E13F75B50E08274BB719"
  };

} // namespace Devv
