/*
 * DevvMessage.h
 * defines the types of messages Devv peers can exchange.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef TYPES_DEVVMESSAGE_H_
#define TYPES_DEVVMESSAGE_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <memory>

#include "common/binary_converters.h"
#include "common/logger.h"
#include "common/util.h"

namespace Devv {

const int num_debug_chars = 8;
const uint8_t kMSG_HEADER = 52;

enum eMessageType {
  FINAL_BLOCK = 0,
  PROPOSAL_BLOCK = 1,
  TRANSACTION_ANNOUNCEMENT = 2,
  VALID = 3,
  REQUEST_BLOCK = 4,
  GET_BLOCKS_SINCE = 5,
  BLOCKS_SINCE = 6,
  NUM_TYPES = 7
};

typedef std::string URI;

struct DevvMessage {
  URI uri;
  eMessageType message_type;
  std::vector<uint8_t> data;
  uint32_t index;

  explicit DevvMessage(uint32_t message_index)
      : uri("")
      , message_type(eMessageType::VALID)
      , data()
      , index(message_index) {}

  DevvMessage(const URI& message_uri,
                 eMessageType msgType,
                 const std::vector<uint8_t>& message_data,
                 uint32_t message_index)
      : uri(message_uri)
      , message_type(msgType)
      , data(message_data)
      , index(message_index) {}

  /**
   * Constructor. Takes a string to initialize data vector
   */
  DevvMessage(const URI& uri,
                 eMessageType msgType,
                 const std::string& data,
                 int index)
    : uri(uri)
    , message_type(msgType)
    , data(data.begin(), data.end())
    , index(index)
  {}

  template <typename T>
  void SetData(const T& object) {
    data.resize(sizeof(object));
    memcpy(data.data(), &object, sizeof(object));
  }

  template <typename T>
  void GetData(T& object, int index = 0) const {
    assert(data.size() >= (sizeof(object) + index));
    memcpy(&object, &data[index], sizeof(object));
  }
};

typedef std::unique_ptr<DevvMessage> DevvMessageUniquePtr;
/// typedef of the DevvMessage callback signature
typedef std::function<void(DevvMessageUniquePtr)> DevvMessageCallback;

/**
 * Append to serialized buffer
 */
template <typename T>
inline void append(std::vector<uint8_t>& buf, const T& obj) {
  unsigned int pos = buf.size();
  buf.resize(pos + sizeof(obj));
  std::memcpy(buf.data()+pos, &obj, sizeof(obj));
}

/**
 * Append to serialized buffer. Specialize for string
 */
template <>
inline void append(std::vector<uint8_t>& buf, const std::string& obj) {
  // Serialize the size of the string
  unsigned int size = obj.size();
  append(buf, size);
  // Serialize the string
  buf.insert(buf.end(), obj.begin(), obj.end());
}

/**
 * Append to serialized buffer. Specialize for data buf
 */
template <>
inline void append(std::vector<uint8_t>& buf, const std::vector<uint8_t>& obj) {
  // Serialize the size of the vector
  unsigned int source_size = obj.size();
  append(buf, source_size);
  // Serialize the vector
  buf.insert(buf.end(), obj.begin(), obj.end());
}

/**
 * Serialize a message into a buffer
 */
static std::vector<uint8_t> serialize(const DevvMessage& msg) {
  // Arbitrary header
  uint8_t header_version = kMSG_HEADER;
  // Serialized buffer
  std::vector<uint8_t> bytes;

  // Serialize a header. Stripped during deserialization
  append(bytes, header_version);
  // Serialize the index
  append(bytes, msg.index);
  // Serialize the message type
  append(bytes, msg.message_type);
  // Serialize the URI
  append(bytes, msg.uri);
  // Serialize the data buffer
  append(bytes, msg.data);

  return bytes;
}

/**
 * Extract data from serialized buffer.
 * @return Index of buffer after extraction
 */
template <typename T>
inline unsigned int extract(T& dest, const std::vector<uint8_t>& buffer, unsigned int index) {
  assert(buffer.size() >= index + sizeof(dest));
  std::memcpy(&dest, &buffer[index], sizeof(dest));
  return(index + sizeof(dest));
}

/**
 * Extract data from serialized buffer. Specialized for std::string
 * @return Index of buffer after extraction
 */
template <>
inline unsigned int extract(std::string& dest,
                            const std::vector<uint8_t>& buffer,
                            unsigned int index) {
  unsigned int size;
  unsigned int new_index = extract(size, buffer, index);
  assert(buffer.size() >= new_index + size);
  dest.append(reinterpret_cast<const char*>(&buffer[new_index]), size);
  return(new_index + size);
}

/**
 * Extract data from serialized buffer. Specialized for std::vector<uint8_t>
 * @return Index of buffer after extraction
 */
template <>
inline unsigned int extract(std::vector<uint8_t>& dest,
                            const std::vector<uint8_t>& buffer,
                            unsigned int index) {
  unsigned int size;
  unsigned int new_index = extract(size, buffer, index);
  assert(buffer.size() >= new_index + size);
  dest.insert(dest.end(), buffer.begin() + new_index, buffer.begin() + new_index + size);
  return(new_index + size);
}

/**
 * Deserialize a a buffer into a devv message
 */
static DevvMessageUniquePtr deserialize(const std::vector<uint8_t>& bytes) {
  unsigned int buffer_index = 0;
  uint8_t header_version = 0;

  // Create the devv message
  auto message = std::make_unique<DevvMessage>(0);

  // Get the header_version
  buffer_index = extract(header_version, bytes, buffer_index);
  assert(header_version == kMSG_HEADER);
  // index
  buffer_index = extract(message->index, bytes, buffer_index);
  // message type
  buffer_index = extract(message->message_type, bytes, buffer_index);
  // URI
  buffer_index = extract(message->uri, bytes, buffer_index);
  // data
  extract(message->data, bytes, buffer_index);

  return(message);
}


static std::string GetMessageType (const DevvMessage& message) {
  std::string message_type_string;
  switch (message.message_type) {
  case(eMessageType::FINAL_BLOCK):
    message_type_string = "FINAL_BLOCK";
    break;
  case(eMessageType::PROPOSAL_BLOCK):
    message_type_string = "PROPOSAL_BLOCK";
    break;
  case(eMessageType::TRANSACTION_ANNOUNCEMENT):
    message_type_string = "TRANSACTION_ANNOUNCEMENT";
    break;
  case(eMessageType::VALID):
    message_type_string = "VALID";
    break;
  case(eMessageType::REQUEST_BLOCK):
    message_type_string = "REQUEST_BLOCK";
    break;
  case(eMessageType::GET_BLOCKS_SINCE):
    message_type_string = "GET_BLOCKS_SINCE";
    break;
  case(eMessageType::BLOCKS_SINCE):
    message_type_string = "BLOCKS_SINCE";
    break;
  default:
    message_type_string = "ERROR_DEFAULT";
    break;
  }
  return message_type_string;
}

/**
 * Stream the message to the logger
 */
  static void LogDevvMessageSummary(const DevvMessage& message,
                                       const std::string& source,
                                       int summary_bytes = num_debug_chars) {

  auto message_type_string = GetMessageType(message);

  std::string summary;
  if ((summary_bytes < 0) || (message.data.size() < (static_cast<size_t>(summary_bytes) * 2))) {
    summary = ToHex(message.data);
  } else {
    std::vector<uint8_t> sub_vec;
    sub_vec.insert(sub_vec.end(), message.data.begin(), message.data.begin() + summary_bytes);
    summary = ToHex(sub_vec);
    summary += "..";
    sub_vec.assign(message.data.end() - summary_bytes,
                   message.data.end());
    summary += ToHex(sub_vec);
  }

  LOG_INFO <<
    "URI: " << message.uri << " | " <<
    "TPE: " << message_type_string << " | " <<
    "SZE: " << message.data.size() << " | " <<
    "IDX: " << message.index << " | " <<
    "SUM: " << summary << " | " <<
    "SRC: " << source;
}

} /* namespace Devv */

#endif /* TYPES_DEVVMESSAGE_H_ */
