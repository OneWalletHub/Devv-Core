/*
 * Tier2Transaction.h defines the structure of the transaction section of a block
 * for Tier 2 shards.
 *
 * @copywrite  2018 Devvio Inc
 *
 */

#ifndef PRIMITIVES_TIER2TRANSACTION_H_
#define PRIMITIVES_TIER2TRANSACTION_H_

#include "Transaction.h"
#include "common/devv_exceptions.h"
#include "common/logger.h"

namespace Devv {

/**
 * The Tier2 representation of a Transaction
 */
class Tier2Transaction : public Transaction {
 public:
  /**
   * Move constructor
   * @param other
   */
   Tier2Transaction(Tier2Transaction&& other) = default;

  /**
   * Default move assignment constructor
   * @param other
   */
   Tier2Transaction& operator=(Tier2Transaction&& other) = default;

  /**
   * Constructor
   * The constructor will sign this transaction after creation
   *
   * @param oper Operation of this transaction
   * @param xfers
   * @param nonce
   * @param eckey
   * @param keys
   */
  Tier2Transaction(byte oper,
                   const std::vector<Transfer>& xfers,
                   std::vector<byte> nonce,
                   EC_KEY* eckey,
                   const KeyRing& keys)
      : Transaction(false) {
    nonce_size_ = nonce.size();

    if (nonce_size_ < minNonceSize()) {
      throw DevvMessageError("Invalid serialized T2 transaction, nonce is too small ("
        + std::to_string(nonce_size_) + ")");
    }

    Uint64ToBin(nonce_size_, canonical_);
    canonical_.push_back(oper);

    xfer_size_ = 0;
    for (const auto& transfer : xfers) {
      xfer_size_ += transfer.Size();
      std::vector<byte> xfer_canon(transfer.getCanonical());
      canonical_.insert(std::end(canonical_), std::begin(xfer_canon), std::end(xfer_canon));
    }

    std::vector<byte> xfer_size_bin;
    Uint64ToBin(xfer_size_, xfer_size_bin);
    //Note that xfer size goes on the beginning
    canonical_.insert(std::begin(canonical_), std::begin(xfer_size_bin), std::end(xfer_size_bin));

    canonical_.insert(std::end(canonical_), std::begin(nonce), std::end(nonce));
    std::vector<byte> msg(getMessageDigest());
    Signature sig = SignBinary(eckey, DevvHash(msg));
    std::vector<byte> sig_canon(sig.getCanonical());
    canonical_.insert(std::end(canonical_), std::begin(sig_canon), std::end(sig_canon));
    is_sound_ = isSound(keys);
    if (!is_sound_) {
      throw DevvMessageError("Invalid serialized T2 transaction, not sound!");
    }
  }

  /**
   * Constructor
   * This constructor will use the provided signature
   *
   * @param oper Operation of this transaction
   * @param xfers
   * @param nonce
   * @param eckey
   * @param keys
   * @param signature
   */
  Tier2Transaction(byte oper,
                   const std::vector<Transfer>& xfers,
                   std::vector<byte> nonce,
                   EC_KEY* eckey,
                   const KeyRing& keys,
                   const Signature& signature)
      : Transaction(false) {
    nonce_size_ = nonce.size();

    if (nonce_size_ < minNonceSize()) {
      LOG_WARNING << "Invalid serialized T2 transaction, nonce is too small ("
        +std::to_string(nonce_size_)+").";
    }

    Uint64ToBin(nonce_size_, canonical_);
    canonical_.push_back(oper);

    xfer_size_ = 0;
    for (const auto& transfer : xfers) {
      xfer_size_ += transfer.Size();
      std::vector<byte> xfer_canon(transfer.getCanonical());
      canonical_.insert(std::end(canonical_), std::begin(xfer_canon), std::end(xfer_canon));
    }

    std::vector<byte> xfer_size_bin;
    Uint64ToBin(xfer_size_, xfer_size_bin);
    //Note that xfer size goes on the beginning
    canonical_.insert(std::begin(canonical_), std::begin(xfer_size_bin), std::end(xfer_size_bin));

    canonical_.insert(std::end(canonical_), std::begin(nonce), std::end(nonce));
    std::vector<byte> msg(getMessageDigest());
    LOG_DEBUG << signature.getJSON();
    std::vector<byte> sig_canon(signature.getCanonical());
    canonical_.insert(std::end(canonical_), std::begin(sig_canon), std::end(sig_canon));
    is_sound_ = isSound(keys);
    if (!is_sound_) {
      throw DevvMessageError("Invalid serialized T2 transaction, not sound!");
    }
  }

  /**
   * Constructor
   * @param oper Operation of this transaction
   * @param xfers
   * @param nonce
   * @param eckey
   */
  Tier2Transaction(byte oper,
                   const std::vector<Transfer>& xfers,
                   std::vector<byte> nonce,
                   EC_KEY* eckey)
      : Transaction(false) {
    nonce_size_ = nonce.size();

    if (nonce_size_ < minNonceSize()) {
      LOG_WARNING << "Invalid serialized T2 transaction, nonce is too small ("
        +std::to_string(nonce_size_)+").";
    }

    Uint64ToBin(nonce_size_, canonical_);
    canonical_.push_back(oper);

    xfer_size_ = 0;
    for (const auto& transfer : xfers) {
      xfer_size_ += transfer.Size();
      std::vector<byte> xfer_canon(transfer.getCanonical());
      canonical_.insert(std::end(canonical_), std::begin(xfer_canon), std::end(xfer_canon));
    }

    std::vector<byte> xfer_size_bin;
    Uint64ToBin(xfer_size_, xfer_size_bin);
    //Note that xfer size goes on the beginning
    canonical_.insert(std::begin(canonical_), std::begin(xfer_size_bin), std::end(xfer_size_bin));

    canonical_.insert(std::end(canonical_), std::begin(nonce), std::end(nonce));
    std::vector<byte> msg(getMessageDigest());
    Signature signature = SignBinary(eckey, DevvHash(msg));
    LOG_DEBUG << signature.getJSON();
    std::vector<byte> sig_canon(signature.getCanonical());
    canonical_.insert(std::end(canonical_), std::begin(sig_canon), std::end(sig_canon));
  }

  /**
   * Constructor
   * @param oper Operation of this transaction
   * @param xfers
   * @param nonce
   * @param signature
   */
  Tier2Transaction(byte oper,
                   const std::vector<Transfer>& xfers,
                   const std::vector<byte>& nonce,
                   const Signature& signature)
      : Transaction(false) {
    nonce_size_ = nonce.size();

    if (nonce_size_ < minNonceSize()) {
      LOG_WARNING << "Invalid serialized T2 transaction, nonce is too small ("
        +std::to_string(nonce_size_)+").";
    }

    Uint64ToBin(nonce_size_, canonical_);
    canonical_.push_back(oper);

    xfer_size_ = 0;
    for (const auto& transfer : xfers) {
      xfer_size_ += transfer.Size();
      std::vector<byte> xfer_canon(transfer.getCanonical());
      canonical_.insert(std::end(canonical_), std::begin(xfer_canon), std::end(xfer_canon));
    }

    std::vector<byte> xfer_size_bin;
    Uint64ToBin(xfer_size_, xfer_size_bin);
    //Note that xfer size goes on the beginning
    canonical_.insert(std::begin(canonical_), std::begin(xfer_size_bin), std::end(xfer_size_bin));

    canonical_.insert(std::end(canonical_), std::begin(nonce), std::end(nonce));
    std::vector<byte> msg(getMessageDigest());
    LOG_DEBUG << signature.getJSON();
    std::vector<byte> sig_canon(signature.getCanonical());
    canonical_.insert(std::end(canonical_), std::begin(sig_canon), std::end(sig_canon));
  }

  static Tier2Transaction Create(InputBuffer& buffer,
                                 const KeyRing& keys,
                                 bool calculate_soundness = true);

  static Tier2Transaction QuickCreate(InputBuffer& buffer);

  static std::unique_ptr<Tier2Transaction> CreateUniquePtr(InputBuffer& buffer,
                                                           const KeyRing& keys,
                                                           bool calculate_soundness = true);

  /**
   * Returns the operation performed by this transfer
   * @return
   */
  byte getOperation() const { return canonical_[kOPERATION_OFFSET]; }

  /**
   * Get the nonce of this Transaction
   * @return
   */
  std::vector<byte> getNonce() const {
	std::vector<byte> nonce(canonical_.begin() + (EnvelopeSize() + xfer_size_)
	  , canonical_.begin() + (EnvelopeSize() + xfer_size_ + nonce_size_));
    return nonce;
  }

  /**
   * Creates a clone (deep copy) of this transaction
   * @return
   */
  std::unique_ptr<Transaction> clone() const override {
    return std::unique_ptr<Transaction>(new Tier2Transaction(*this));
  }

  /// Declare make_unique<>() as a friend
  friend std::unique_ptr<Tier2Transaction> std::make_unique<Tier2Transaction>();

 private:
  // The bytesize of Transfers in this Transaction
  uint64_t xfer_size_ = 0;
  // The size of this Transaction's nonce
  uint64_t nonce_size_ = 0;

  /**
   * Private default constructor
   */
  Tier2Transaction() = default;

  /**
   * Default private copy constructor. We keep this private so the programmer has to
   * explicity call clone() to avoid accidental copies
   */
   Tier2Transaction(const Tier2Transaction& other) = default;

   /**
    * Fill the given Tier2Transaction from the InputBuffer
    * without providing keys or checking for soundness
    * @param tx
    * @param buffer
    */
   static void QuickFill(Tier2Transaction& tx,
                    InputBuffer &buffer);

   /**
    * Fill the given Tier2Transaction from the InputBuffer
    * @param tx
    * @param buffer
    * @param keys
    * @param calculate_soundness
    */
   static void Fill(Tier2Transaction& tx,
                    InputBuffer &buffer,
                    const KeyRing &keys,
                    bool calculate_soundness);
  /**
   * Return a copy of the message digest
   * @return message digest
   */
  std::vector<byte> do_getMessageDigest() const override {
    /// @todo(mckenney) can this be a reference?
    std::vector<byte> md(canonical_.begin(), canonical_.begin()
      + (EnvelopeSize() + nonce_size_ + xfer_size_));
    return md;
  }

  /**
   * Create and return a vector of Transfers in this transaction
   * @return vector of Transfer objects
   */
  std::vector<TransferPtr> do_getTransfers() const {
    std::vector<TransferPtr> out;
    size_t offset = 0;
    while (offset < xfer_size_) {
      InputBuffer buffer(canonical_, transferOffset() + offset);
      TransferPtr t = std::make_unique<Transfer>(buffer);
      offset += t->Size();
      out.push_back(std::move(t));
    }
    return out;
  }

  /**
   * Create and return the signature of this transaction
   * @return the signature of this transaction
   */
  Signature do_getSignature() const {
    if (getOperation() == eOpType::Exchange) {
      std::vector<byte> sig_bin(canonical_.begin() + (EnvelopeSize() + nonce_size_ + xfer_size_)
	      ,canonical_.begin() + (EnvelopeSize() + nonce_size_ + xfer_size_) + kWALLET_SIG_BUF_SIZE);
	  Signature sig(sig_bin);
      return sig;
	} else {
      std::vector<byte> sig_bin(canonical_.begin() + (EnvelopeSize() + nonce_size_ + xfer_size_)
	      ,canonical_.begin() + (EnvelopeSize() + nonce_size_ + xfer_size_) + kNODE_SIG_BUF_SIZE);
	  Signature sig(sig_bin);
      return sig;
	}
  }

  /**
   * Compute and set the soundness
   * @param keys KeyRing to use to check soundness
   * @return true if this Transaction is sound
   */
  bool do_setIsSound(const KeyRing& keys) {
    is_sound_ = isSound(keys);
    if (!is_sound_) {
      LOG_WARNING << "Invalid serialized transaction, not sound!";
    }
    return is_sound_;
  }

  /**
   * Return soundness. If is_sound_ is not true, calculate soundness
   * @param keys KeyRing to use to compute soundness
   * @return true if sound
   */
  bool do_isSound(const KeyRing& keys) const {
    MTR_SCOPE_FUNC();
    try {
      if (is_sound_) { return (is_sound_); }
      long total = 0;
      byte oper = getOperation();

      bool sender_set = false;
      Address sender;

      std::vector<TransferPtr> xfers = getTransfers();
      for (auto& it : xfers) {
        int64_t amount = it->getAmount();
        total += amount;
        if (amount < 0) {
          if (sender_set) {
            if (sender != it->getAddress()) {
              throw std::runtime_error("TransactionError: Multiple senders in transaction");
            } else {
              LOG_INFO << "Sending multiple distinct transfers at once.";
            }
          }
          sender = it->getAddress();
          sender_set = true;
        }
      }
      if (oper == eOpType::Create || oper == eOpType::Delete || oper == eOpType::Modify) {
        if (!keys.isINN(sender)) {
          throw std::runtime_error("TransactionError: Invalid operation, non-INN address performing privileged operation.");
        }
      }
      if (total != 0) {
        std::string err = "TransactionError: transaction amounts are asymmetric. (sum=" + std::to_string(total) + ")";
        throw std::runtime_error(err);
      }

      if ((oper != eOpType::Exchange) && (!keys.isINN(sender))) {
        throw std::runtime_error("TransactionError: INN transaction not performed by INN");
      }

      EC_KEY* eckey = keys.getKey(sender);
      std::vector<byte> msg(getMessageDigest());
      Signature sig = getSignature();

      auto verified = VerifyByteSig(eckey, DevvHash(msg), sig);
      EC_KEY_free(eckey);

      if (!verified) {
        std::string err = "TransactionError: transaction signature did not validate";
        LOG_DEBUG << "Transaction state is: " + getJSON();
        LOG_DEBUG << "Sender addr is: " + sender.getJSON();
        LOG_DEBUG << "Signature is: " + sig.getJSON();
        throw std::runtime_error(err);
      }
      return true;
    } catch (const std::exception& e) {
      LOG_WARNING << FormatException(&e, "transaction");
      throw;
    }
  }

  /**
   * Compute whether or not this Transaction is valid
   * @param state
   * @param keys
   * @param summary
   * @return
   */
  bool do_isValid(ChainState& state, const KeyRing& keys, Summary& summary) const override {
    try {
      if (!isSound(keys)) {
        return false;
      }
      byte oper = getOperation();
      std::vector<TransferPtr> xfers = getTransfers();
      for (auto& it : xfers) {
        int64_t amount = it->getAmount();
        uint64_t coin = it->getCoin();
        Address addr = it->getAddress();
        //LOG_DEBUG << "STATE (amt/coin/tot/address): " << amount << "/" << coin << "/" << state.getAmount(coin, addr)<< "/"<< addr.getHexString();
        if (amount < 0) {
          if ((oper == eOpType::Exchange) && (std::abs(amount) > state.getAmount(coin, addr))) {
            LOG_WARNING << "Coins not available at addr: amount(" << amount
                        << "), state.getAmount()(" << state.getAmount(coin, addr) << ")";
            return false;
          } else {
            LOG_INFO << "eOpType(" << int(oper) << "): addr(" << addr.getHexString() << "): amount: " << amount
                     << " state.getAmount(): " << state.getAmount(coin, addr);
          }
        }
        SmartCoin next_flow(addr, coin, amount);
        state.addCoin(next_flow);
        summary.addItem(addr, coin, amount, it->getDelay());
        LOG_INFO << "New balance: wallet address: " << addr.getHexString()
                 << "; coin: " << coin << "; balance: " << state.getAmount(coin, addr);
      }
      return true;
    } catch (const std::exception& e) {
      LOG_WARNING << FormatException(&e, "transaction");
    }
    return false;
  }

  /**
   * Checks if this transaction is valid with respect to a chain state and other transactions.
   * Transactions are atomic, so if any portion of the transaction is invalid,
   * the entire transaction is also invalid.
   * If this transaction is invalid given the transfers implied by the aggregate map,
   * then this function returns false.
   * @note if this transaction is valid based on the chainstate, aggregate is updated
   *       based on the signer of this transaction.
   * @param state the chain state to validate against
   * @param keys a KeyRing that provides keys for signature verification
   * @param summary the Summary to update
   * @param aggregate, coin sending information about parallel transactions
   * @param prior, the chainstate before adding any new transactions
   * @return true iff the transaction is valid in this context, false otherwise
   */
  bool do_isValidInAggregate(ChainState& state, const KeyRing& keys
       , Summary& summary, std::map<Address, SmartCoin>& aggregate
       , const ChainState& prior) const override {
    try {
      if (!isSound(keys)) {
        return false;
      }
      byte oper = getOperation();
      std::vector<TransferPtr> xfers = getTransfers();
      bool no_error = true;
      LOG_DEBUG << "do_isValidInAggregate() state.getStateMap().size():  " << state.getStateMap().size();
      for (auto& it : xfers) {
        int64_t amount = it->getAmount();
        uint64_t coin = it->getCoin();
        Address addr = it->getAddress();
        if (amount < 0) {
          if ((oper == eOpType::Exchange) && (std::abs(amount) > state.getAmount(coin, addr))) {
            LOG_WARNING << "Coins not available at addr("<<addr.getHexString()<<"): amount(" << amount
                        << "), state.getAmount()(" << state.getAmount(coin, addr) << ")";
            return false;
          } else {
            LOG_DEBUG << "eOpType(" << int(oper) << "): addr(" << addr.getHexString() << "): amount: " << amount
                     << " state.getAmount(): " << state.getAmount(coin, addr);
          }
          auto it = aggregate.find(addr);
          if (it != aggregate.end()) {
            int64_t historic = prior.getAmount(coin, addr);
            int64_t committed = it->second.getAmount();
            //if sum of negative transfers < 0 a bad ordering is possible
            if ((historic+committed+amount) < 0) return false;
            SmartCoin sc(addr, coin, amount+committed);
            it->second = sc;
          } else {
            SmartCoin sc(addr, coin, amount);
            auto result = aggregate.insert(std::make_pair(addr, sc));
            no_error = result.second && no_error;
		  }
        }
        SmartCoin next_flow(addr, coin, amount);
        state.addCoin(next_flow);
        summary.addItem(addr, coin, amount, it->getDelay());
        LOG_INFO << "New balance: wallet address: " << addr.getHexString()
                 << "; coin: " << coin << "; balance: " << state.getAmount(coin, addr);
      }
      return no_error;
    } catch (const std::exception& e) {
      LOG_WARNING << FormatException(&e, "transaction");
    }
    return false;
  }

  /**
   * Returns a JSON string representing this transaction.
   * @return a JSON string representing this transaction.
   */
  std::string do_getJSON() const override {
    std::string json("{\"" + kXFER_SIZE_TAG + "\":");
    json += std::to_string(xfer_size_) + ",";
    json += "\"" + kNONCE_SIZE_TAG + "\":"+std::to_string(nonce_size_)+",";
    json += "\"" + kOPER_TAG + "\":" + std::to_string(getOperation()) + ",";
    json += "\"" + kXFER_TAG + "\":[";
    bool isFirst = true;
    std::vector<TransferPtr> xfers = getTransfers();
    for (auto& it : xfers) {
      if (isFirst) {
        isFirst = false;
      } else {
        json += ",";
      }
      json += it->getJSON();
    }
    json += "],\"" + kNONCE_TAG + "\":\"" + ToHex(getNonce()) + "\",";
    Signature sig = getSignature();
    json += "\"" + kSIG_TAG + "\":\"" + sig.getJSON() + "\"}";
    return json;
  }
};

typedef std::unique_ptr<Tier2Transaction> Tier2TransactionPtr;
}  // end namespace Devv

#endif  // DEVV_PRIMITIVES_TRANSACTION_H
