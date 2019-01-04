/*
 * devv-scanner.cpp the main class for a block scanner.
 * @note the mode must be set appropriately
 * Use T1 to scan a tier 1 chain, T2 to scan a T2 chain, and scan to scan raw transactions.
 *
 * Scans and checks public keys without using any private keys.
 * Handles each file independently, so blockchains should be in a single file.
 *
 * @copywrite  2018 Devvio Inc
 */

#include <string>
#include <iostream>
#include <fstream>
#include <functional>
#include <thread>
#include <ctime>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "common/devv_context.h"
#include "modules/BlockchainModule.h"
#include "primitives/json_interface.h"
#include "primitives/block_tools.h"
#include "common/logger.h"

using namespace Devv;
namespace fs = boost::filesystem;
namespace bpt = boost::property_tree;

struct scanner_options {
  eAppMode mode  = eAppMode::T1;
  std::string working_dir;
  std::string write_file;
  std::string address_filter;
  eDebugMode debug_mode = eDebugMode::off;
  unsigned int version = 0;
};

const std::string red      = "\033[31m";
const std::string green    = "\033[32m";
const std::string yellow   = "\033[33m";
const std::string blue     = "\033[34m";
const std::string magenta  = "\033[35m";
const std::string cyan     = "\033[36m";
const std::string white    = "\033[37m";


const std::string bold_red      = "\033[1;31m";
const std::string bold_green    = "\033[1;32m";
const std::string bold_yellow   = "\033[1;33m";
const std::string bold_blue     = "\033[1;34m";
const std::string bold_magenta  = "\033[1;35m";
const std::string bold_cyan     = "\033[1;36m";
const std::string bold_white    = "\033[1;37m";

const std::string bold_bright_green = "\033[1;92m";
const std::string bold_bright_magenta = "\033[1;95m";
const std::string bold_bright_cyan = "\033[1;96m";

const std::string reset    = "\033[0m";

std::unique_ptr<struct scanner_options> ParseScannerOptions(int argc, char** argv);

/**
 * Convert the input string to a json ptree and add it to the supplied tree
 *
 * @param input_string Input JSON string containing FinalBlocks
 * @param tree Tree in which to add the new FinalBlock
 * @param block_count The block index number
 */
void add_to_tree(const std::string& input_string, bpt::ptree& tree, size_t block_count) {
  bpt::ptree pt2;
  std::istringstream is(input_string);
  bpt::read_json(is, pt2);
  std::string key = "block_" + std::to_string(block_count);
  //tree.put_child(key, pt2);
  tree.push_back(std::make_pair("", pt2));
}

/**
 * Filters the ptree (array of blocks) and returns a ptree with a list
 * of transactions that contain a transfer involving the provided
 * address
 *
 * @param tree A ptree of blocks to search in
 * @param filter_address The address to search for
 * @return New ptree containing list of matched addresses
 */
bpt::ptree filter_by_address(const bpt::ptree& tree, const std::string& filter_address) {
  bpt::ptree tx_tree;
  bpt::ptree tx_children;
  for (auto block : tree) {
    const bpt::ptree txs = block.second.get_child("txs");
    for (auto tx : txs) {
      const bpt::ptree transfers = tx.second.get_child("xfer");
      bool addr_found = false;
      for (auto xfer : transfers) {
        auto address = xfer.second.find("addr");
        std::string addr = address->second.data();
        std::size_t found = addr.find(filter_address);
        if (found != std::string::npos) {
          addr_found = true;
        }
      }
      if (addr_found) {
        tx_children.push_back(std::make_pair("", tx.second));
      }
    }
  }
  tx_tree.add_child("txs", tx_children);
  return tx_tree;
}

/**
 *
 * @param hash
 * @return
 */
 template <typename ARRAY>
std::string GetSummary2(const ARRAY& hash, size_t summary_bytes = 6) {
  //auto hex_hash = ToHex(hash);
  auto hex_hash = hash;
  std::string summary;
  summary.insert(summary.end(), hex_hash.begin(), hex_hash.begin() + summary_bytes);
  summary += "..";
  summary.insert(summary.end(),
                 hex_hash.begin() + (hex_hash.size()/2)-3,
                 hex_hash.begin() + (hex_hash.size()/2)+3);
  summary += "..";
  summary.insert(summary.end(), hex_hash.end() - summary_bytes, hex_hash.end());
  return summary;
}

template <typename ARRAY>
std::string GetSummary(const ARRAY& hash, size_t summary_bytes = 6) {
  std::string summary;
  summary.insert(summary.end(), hash.begin(), hash.begin() + summary_bytes);
  summary += "...";
  summary.insert(summary.end(), hash.end() - summary_bytes, hash.end());
  return summary;
}

/*
 * Block Viewer
 */
class BlockViewer {
  struct tx_summary_structure {
    std::string separator = "|";
    std::string empty = "-";
    int number = 8;
    int oper = 10;
    size_t address_size = 68;
    int type = 6;
    int amount = 12;
    int nonce = 29;
    int sig = 31;

    std::string frame_color = green;
    std::string metric_color = bold_cyan;
    std::string value_color = bold_white;
    std::string title_color = bold_bright_green;

    int totalWidth() {
      int width = number + oper + address_size + type + amount + nonce + sig;
      // add for separators and spaces
      width += 7*2+1;
      return width;
    }

    /**
     *
     * @return
     */
    std::string getHorizonal() {
      std::string h(totalWidth()-2, '-');
      return h;
    }

    std::string getTitleLine() {
      std::string title = " Recent Transaction Summaries";
      std::string spaces(totalWidth()-title.size()-2, ' ');
      std::string out = frame_color + "|" + title_color + title + spaces + reset + frame_color + "|";
      return out;
    }

    void putTx(std::ostream& dest,
               size_t tx_num,
               eOpType oper,
               const Address& address,
               uint64_t coin_type,
               int64_t amount,
               const std::vector<byte>& nonce,
               const Signature& sig) {
      std::string address_color = (amount > 0) ? green : white;

      auto this_address = (address.getHexString().size() > this->address_size) ? GetSummary(address.getHexString(), 24)
                                                                                 : address.getHexString();
      dest << this->separator << std::setw(this->number) << tx_num << " "
           << this->separator << std::setw(this->oper) << getOpName(oper) << " "
           << this->separator << address_color << std::setw(this->address_size) << this_address << reset << " "
           << this->separator << std::setw(this->type) << coin_type << " "
           << this->separator << address_color << std::setw(this->amount) << amount << reset << " "
           << this->separator << std::setw(this->nonce) << GetSummary(ToHex(nonce)) << " "
           << this->separator << std::setw(this->sig) << GetSummary(sig.getJSON()) << " "
           << this->separator << std::endl;
    }
  };

  /**
   *
   */
  struct chain_summary_structure {
    std::string separator = "|";
    std::string empty = "-";

    std::string shard_name = "My Shard";

    int width = 40;

    std::string frame_color = cyan;
    std::string metric_color = bold_cyan;
    std::string value_color = bold_white;
    std::string title_color = bold_bright_cyan;

    std::string pre_line = reset + frame_color + separator + " ";
    std::string post_line = reset + " " + frame_color + separator + reset;
    std::vector<std::string> lines;

    std::string getSummaryLine(const std::string& metric, const std::string& value, const std::string& value_color) {
      size_t metric_width = metric.size() + value.size();
      std::string spaces = std::string(width - metric_width-4, ' ');

      std::string line = pre_line + metric_color + metric + spaces + value_color + value + post_line;
      return line;
    }

    std::string getFilledLine(char filler) {
      std::string spaces = std::string(width-4, filler);
      return(pre_line + spaces + post_line);
   }

    void generateSummary() {
      lines.clear();
      // Title
      lines.push_back(getSummaryLine("Shard Name", "My Shard", value_color));
      lines.push_back(getSummaryLine("Creation Time", "Jan 3, 2019", value_color));
      lines.push_back(getSummaryLine("Shard Age", "3.2 hours", value_color));
      lines.push_back(getSummaryLine("Num validators", "3", value_color));
      lines.push_back(getFilledLine('-'));
      lines.push_back(getFilledLine(' '));
      lines.push_back(getSummaryLine("Latest Block Number", "245", value_color));
      lines.push_back(getSummaryLine("Chain Size (KB)", "1434", value_color));
      lines.push_back(getSummaryLine("Largest Block Size (KB)", "12", value_color));
      lines.push_back(getFilledLine('-'));
      lines.push_back(getSummaryLine("Latest Tx Number", "My Shard", value_color));
      lines.push_back(getSummaryLine("Total Tx Volume", "My Shard", value_color));
      lines.push_back(getFilledLine('-'));
      lines.push_back(getFilledLine('9'));

      // Start time
      // Num blocks
      // total chain size

    }
     /**
     *
     * @return
     */
    std::string getHorizonal() {
      std::string h(this->width-2, '-');
      return h;
    }

    std::string getTitleLine() {
      std::string title = " Chain Status";
      std::string spaces(width-title.size()-2, ' ');
      std::string out = frame_color + "|" + title_color + title + spaces + reset + frame_color + "|";
      return out;
    }

  };

  /**
   *
   */
  struct block_summary_structure {
    std::string separator = "|";
    std::string empty = "-";
    int height = 8;
    int date = 28;
    int txs = 8;
    int prev = 28;
    int merkle = 28;
    int volume = 14;
    int size = 10;

    int summary_bytes = 6;

    std::string frame_color = magenta;
    std::string title_color = bold_bright_magenta;
    std::string header_color = bold_blue;
    std::string text_color = bold_white;

    /**
     *
     * @return
     */
    int totalWidth() {
      int width = height + date + txs + prev + merkle + volume + size;
      // add for separators and spaces
      width += 7*2+1;
      return width;
    }

    /**
     *
     * @return
     */
    std::string getHorizonal() {
      std::string h(totalWidth()-2, '-');
      return h;
    }

    std::string getTitleLine() {
      std::string title = " Recent Block Summaries";
      std::string spaces(totalWidth()-title.size()-2, ' ');
      std::string out = frame_color + "|" + title_color + title + spaces + reset + frame_color + "|";
      return out;
    }
    /**
     *
     * @param hash
     * @return
     */
    std::string getSummary(const Hash& hash) {
      auto hex_hash = ToHex(hash);
      std::string summary;
      summary.insert(summary.end(), hex_hash.begin(), hex_hash.begin() + summary_bytes);
      summary += "..";
      summary.insert(summary.end(),
                     hex_hash.begin() + (hex_hash.size()/2)-3,
                     hex_hash.begin() + (hex_hash.size()/2)+3);
      summary += "..";
      summary.insert(summary.end(), hex_hash.end() - summary_bytes, hex_hash.end());
      return summary;
    }

    /**
     *
     * @param stream
     * @param height
     * @param date
     * @param num_transactions
     * @param previous
     * @param merkle
     * @param volume
     * @param size
     */
    void putBlockLine(std::ostream& stream,
                             uint32_t height,
                             time_t& date,
                             size_t num_transactions, 
                             const Hash& previous,
                             const Hash& merkle,
                             int32_t volume, 
                             size_t size) {
      std::string date_str(ctime(&date));

      date_str.erase(std::remove(date_str.begin(), date_str.end(), '\n'), date_str.end());

      stream << frame_color << separator << text_color  << std::setw(this->height) << height << " "
             << frame_color << separator << text_color  << std::setw(this->date) << date_str << " "
             << frame_color << separator << text_color  << std::setw(this->txs) << num_transactions << " "
             << frame_color << separator << text_color  << std::setw(this->prev) << getSummary(previous) << " "
             << frame_color << separator << text_color  << std::setw(this->merkle) << getSummary(merkle) << " "
             << frame_color << separator << text_color  << std::setw(this->volume) << volume << " "
             << frame_color << separator << text_color  << std::setw(this->size) << size << " "
             << frame_color << separator << text_color << reset;
    }

    /**
     *
     * @param stream
     */
    void putEmptyBlockLine(std::ostream& stream) {
      stream << frame_color << separator << text_color << std::setw(this->height) << this->empty << " "
             << frame_color << separator << text_color << std::setw(this->date) << this->empty << " "
             << frame_color << separator << text_color << std::setw(this->txs) << this->empty << " "
             << frame_color << separator << text_color << std::setw(this->prev) << this->empty << " "
             << frame_color << separator << text_color << std::setw(this->merkle) << this->empty << " "
             << frame_color << separator << text_color << std::setw(this->volume) << this->empty << " "
             << frame_color << separator << text_color << std::setw(this->size) << this->empty << " "
             << frame_color << separator << reset;
    }
  };

 public:
  BlockViewer(std::string chain_name = "view me")
      : chain_(chain_name)
  {
  }

  ~BlockViewer() = default;

  void addBlock(FinalBlockSharedPtr block) {
    chain_.push_back(block);
  }

  /**
   *
   * @param block
   * @param height
   */
  void putBlock(const FinalBlock& block, size_t height) {
    // Convert to seconds and then to time_t to get date
    time_t date = block.getBlockTime()/1000;

    blk_sum_.putBlockLine(output_stream_,
                          height,
                          date,
                          block.getNumTransactions(),
                          block.getPreviousHash(),
                          block.getMerkleRoot(),
                          0,
                          block.getNumBytes());
  }

  /**
   *
   * @param tx
   * @param tx_num
   */
  void putTx(const Tier2Transaction& tx, size_t block_num) {
    auto transfers = tx.getTransfers();
    //output_stream_ << tx_sum_.separator << tx_sum_.getHorizonal() << tx_sum_.separator << std::endl;
    for (auto& transfer : transfers) {
      Address address;
      if (transfer->getAmount() > 0) {
        address = transfer->getAddress();
      } else {
        address = transfer->getAddress();
      }

      tx_sum_.putTx(output_stream_,
                    block_num,
                    static_cast<eOpType>(tx.getOperation()),
                    address,
                    tx.getTransfers().at(0)->getCoin(),
                    transfer->getAmount(),
                    tx.getNonce(),
                    tx.getSignature());
    }
    output_stream_ << tx_sum_.separator << tx_sum_.getHorizonal() << tx_sum_.separator << std::endl;
  }

  /**
   *
   * @param dest
   */
  void dumpBlockHeader(std::ostream& dest) {
    dest << blk_sum_.frame_color <<  blk_sum_.separator << std::setw(blk_sum_.height) << blk_sum_.header_color << "Height" << reset << " "
         << reset << blk_sum_.frame_color <<  blk_sum_.separator << std::setw(blk_sum_.date) << blk_sum_.header_color << "Date" << " "
        << reset << blk_sum_.frame_color <<  blk_sum_.separator << std::setw(blk_sum_.txs) << blk_sum_.header_color << "Txs" << " "
        << reset << blk_sum_.frame_color <<  blk_sum_.separator << std::setw(blk_sum_.merkle) << blk_sum_.header_color << "Previous" << " "
        << reset << blk_sum_.frame_color <<  blk_sum_.separator << std::setw(blk_sum_.merkle) << blk_sum_.header_color << "Merkle" << " "
        << reset << blk_sum_.frame_color <<  blk_sum_.separator << std::setw(blk_sum_.volume) << blk_sum_.header_color << "Volume/Total" << " "
        << reset << blk_sum_.frame_color <<  blk_sum_.separator << std::setw(blk_sum_.size) << blk_sum_.header_color << "Size/Total" << " "
        << reset << blk_sum_.frame_color <<  blk_sum_.separator << reset;
  }

  /**
   *
   * @param dest
   */
  void dumpTxHeader(std::ostream& dest) {
    dest << tx_sum_.frame_color << tx_sum_.separator << tx_sum_.getHorizonal() << tx_sum_.separator << std::endl;
    dest << tx_sum_.frame_color << tx_sum_.separator << bold_blue << std::setw(tx_sum_.number) << "Block" << reset << " "
         << tx_sum_.frame_color << tx_sum_.separator << bold_blue << std::setw(tx_sum_.oper) << "Operation" << reset << " "
         << tx_sum_.frame_color << tx_sum_.separator << bold_blue << std::setw(tx_sum_.address_size-16) << "Address (" << bold_white << "Sender"
                                                           << bold_blue << "/" << bold_green << "Receiver" << bold_blue << ")" << reset << " "
         << tx_sum_.frame_color << tx_sum_.separator << bold_blue << std::setw(tx_sum_.type) << "Type" << reset << " "
         << tx_sum_.frame_color << tx_sum_.separator << bold_blue << std::setw(tx_sum_.amount) << "Amount" << reset << " "
         << tx_sum_.frame_color << tx_sum_.separator << bold_blue << std::setw(tx_sum_.nonce) << "Nonce" << reset << " "
         << tx_sum_.frame_color << tx_sum_.separator << bold_blue << std::setw(tx_sum_.sig) << "Sig" << reset << " "
         << tx_sum_.frame_color << tx_sum_.separator << std::endl;
    dest << tx_sum_.frame_color << tx_sum_.separator << tx_sum_.getHorizonal() << tx_sum_.separator << std::endl;
  }

  /**
   *
   * @param dest
   */
  void fillBlocks(std::ostream& dest, size_t& sum_counter) {
    for (auto i = 0; i < static_cast<int>(num_blocks); i++) {
      int at = chain_.size() - i - 1;
      if (at < 0) {
        blk_sum_.putEmptyBlockLine(dest);
      } else {
        putBlock(*chain_.at(at), at);
      }
      dest << chain_sum_.lines[sum_counter++] << std::endl;
    }
  }

  /**
   *
   * @param dest
   */
  void fillTransactions(std::ostream& dest) {
    std::list<Tier2TransactionPtr> txs;
    for (auto i = 0; i < static_cast<int>(num_blocks); i++) {
      int at = chain_.size() - i - 1;
      if (at < 0) {
        //blk_sum_.putEmptyBlockLine(dest);
      } else {
        auto block = chain_.at(at);
        auto& tx_vec = block->getTransactions();
        auto t2 = dynamic_cast<Tier2Transaction*>(tx_vec.at(0).get());
        putTx(*t2, at);
      }
    }
  }

  /**
   *
   * @param dest
   */
  void dump(std::ostream& dest) {
    chain_sum_.generateSummary();

    dest << block_frame_color_ << blk_sum_.separator << blk_sum_.getHorizonal() << blk_sum_.separator << reset;
    dest << chain_sum_.frame_color << chain_sum_.separator << chain_sum_.getHorizonal() << chain_sum_.separator << reset << std::endl;

    dest << blk_sum_.getTitleLine();
    dest << chain_sum_.getTitleLine();
    dest << std::endl;

    dest << block_frame_color_ << blk_sum_.separator << blk_sum_.getHorizonal() << blk_sum_.separator << reset;
    dest << chain_sum_.frame_color << chain_sum_.separator << chain_sum_.getHorizonal() << chain_sum_.separator << reset << std::endl;

    size_t sum_counter = 0;
    dumpBlockHeader(dest);
    dest << chain_sum_.lines.at(sum_counter++);
    dest << std::endl;

    dest << blk_sum_.separator << blk_sum_.getHorizonal() << blk_sum_.separator;
    dest << chain_sum_.lines.at(sum_counter++) << std::endl;

    fillBlocks(dest, sum_counter);
    dest << block_frame_color_ << blk_sum_.separator << blk_sum_.getHorizonal() << blk_sum_.separator << reset;
    dest << chain_sum_.lines.at(sum_counter++) << std::endl;

    dest << tx_sum_.frame_color << tx_sum_.separator << tx_sum_.getHorizonal() << tx_sum_.separator << std::endl;
    dest << tx_sum_.getTitleLine() << reset << std::endl;
    dumpTxHeader(dest);
    fillTransactions(dest);
    //putBlock(*chain_.back(), 0);
  }

  void dump() {
    dump(output_stream_);
  }

 private:
  Blockchain chain_;
  std::ostream& output_stream_ = std::cout;
  block_summary_structure blk_sum_;
  tx_summary_structure tx_sum_;
  size_t num_blocks = 10;

  chain_summary_structure chain_sum_;
  std::string block_frame_color_ = magenta;
  std::string summary_frame_color_ = cyan;

  std::string separator_ = "|";
  int chain_summary_width_ = 40;
};

int main(int argc, char* argv[])
{
  init_log();

  BlockViewer block_viewer;

  CASH_TRY {
    auto options = ParseScannerOptions(argc, argv);

    if (!options) {
      exit(-1);
    }

    size_t block_counter = 0;
    size_t tx_counter = 0;
    size_t tfer_count = 0;
    size_t total_volume = 0;

    KeyRing keys;
    ChainState priori;
    ChainState posteri;
    Hash prev_hash = DevvHash({'G', 'e', 'n', 'e', 's', 'i', 's'});
    std::set<Signature> dupe_check;
    bpt::ptree block_array;

    fs::path p(options->working_dir);
    if (p.empty()) {
      LOG_WARNING << "Invalid path: "+options->working_dir;
      return(false);
    }

    if (!is_directory(p)) {
      LOG_ERROR << "Error opening dir: " << options->working_dir << " is not a directory";
      return false;
    }

    std::vector<std::string> segments;
    for (auto& entry : boost::make_iterator_range(fs::directory_iterator(p), {})) {
      segments.push_back(entry.path().string());
    }
    std::sort(segments.begin(), segments.end());
    for (auto const& seg_path : segments) {
      LOG_DEBUG << "Enter segment: " << seg_path;
      fs::path seg(seg_path);
      if (!seg.empty() && is_directory(seg)) {
        std::vector<std::string> files;

        for(auto& seg_entry : boost::make_iterator_range(fs::directory_iterator(seg), {})) {
          files.push_back(seg_entry.path().string());
        }
        std::sort(files.begin(), files.end());

        for  (auto const& file_name : files) {
          LOG_DEBUG << "Reading " << file_name;
          std::ifstream file(file_name, std::ios::binary);
          file.unsetf(std::ios::skipws);
          std::size_t file_size;
          file.seekg(0, std::ios::end);
          file_size = file.tellg();
          file.seekg(0, std::ios::beg);

          std::vector<byte> raw;
          raw.reserve(file_size);
          raw.insert(raw.begin(), std::istream_iterator<byte>(file), std::istream_iterator<byte>());
          bool is_block = IsBlockData(raw);
          bool is_transaction = IsTxData(raw);
          if (is_block) { LOG_INFO << file_name << " has blocks."; }
          if (is_transaction) { LOG_INFO << file_name << " has transactions."; }
          if (!is_block && !is_transaction) {
            LOG_WARNING << file_name << " contains unknown data.";
          }
          size_t file_blocks = 0;
          size_t file_txs = 0;
          size_t file_tfer = 0;
          uint64_t start_time = 0;
          uint64_t blocktime = 0;
          uint64_t volume = 0;
          uint64_t value = 0;

          InputBuffer buffer(raw);
          while (buffer.getOffset() < static_cast<size_t>(file_size)) {
            try {
              if (is_transaction) {
                Tier2Transaction tx(Tier2Transaction::Create(buffer, keys, true));
                file_txs++;
                file_tfer += tx.getTransfers().size();

                add_to_tree(tx.getJSON(), block_array, block_counter);
              } else if (is_block) {
                auto one_block_ptr = std::make_shared<FinalBlock>(buffer, priori, keys, options->mode);
                FinalBlock& one_block = *one_block_ptr;
                block_viewer.addBlock(one_block_ptr);
                //FinalBlock one_block(buffer, priori, keys, options->mode);

                if (one_block.getVersion() != options->version) {
                  LOG_WARNING << "Unexpected block version ("+std::to_string(one_block.getVersion())+") in " << file_name;
                }
                size_t txs_count = one_block.getNumTransactions();
                size_t tfers = one_block.getNumTransfers();
                uint64_t previous_time = blocktime;
                blocktime = one_block.getBlockTime();
                uint64_t duration = blocktime-previous_time;
                priori = one_block.getChainState();
                Hash p_hash = one_block.getPreviousHash();
                if (!std::equal(std::begin(prev_hash), std::end(prev_hash), std::begin(p_hash))) {
                  LOG_WARNING << "CHAINBREAK: The previous hash referenced in this block does not match the previous block hash.";
                  LOG_WARNING << "Expected Hash: " << ToHex(prev_hash);
                  LOG_WARNING << "Found Hash: " << ToHex(p_hash);
                }
                prev_hash = DevvHash(one_block.getCanonical());

                add_to_tree(GetJSON(one_block), block_array, block_counter);

                LOG_INFO << std::to_string(txs_count)+" txs, transfers: "+std::to_string(tfers);
                LOG_INFO << "Duration: "+std::to_string(duration)+" ms.";
                if (duration != 0 && previous_time != 0) {
                  LOG_INFO << "Rate: "+std::to_string(txs_count*1000/duration)+" txs/sec";
                } else if (previous_time == 0) {
                  start_time = blocktime;
                }
                uint64_t block_volume = one_block.getVolume();
                volume += block_volume;
                value += one_block.getValue();
                LOG_INFO << "Volume: "+std::to_string(block_volume);
                LOG_INFO << "Value: "+std::to_string(one_block.getValue());

                Summary block_summary(Summary::Create());
                std::vector<TransactionPtr> txs = one_block.CopyTransactions();
                for (TransactionPtr& item : txs) {
                  if (!item->isValid(posteri, keys, block_summary)) {
                    LOG_WARNING << "A transaction is invalid. TX details: ";
                    LOG_WARNING << item->getJSON();
                  }
                  auto ret = dupe_check.insert(item->getSignature());
                  if (ret.second==false) {
                    LOG_WARNING << "DUPLICATE TRANSACTION detected: "+item->getSignature().getJSON();
                  }
                }
                if (block_summary.getCanonical() != one_block.getSummary().getCanonical()) {
                  LOG_WARNING << "A final block summary is invalid. Summary datails: ";
                  LOG_WARNING << GetJSON(one_block.getSummary());
                  LOG_WARNING << "Transaction details: ";
                  for (TransactionPtr& item : txs) {
                    LOG_WARNING << item->getJSON();
                  }
                }

                file_blocks++;
                file_txs += txs_count;
                file_tfer += tfers;
              } else {
                LOG_WARNING << "!is_block && !is_transaction";
              }
            } catch (const std::exception& e) {
              LOG_ERROR << "Error scanning " << file_name << " skipping to next file.  Error details: "+FormatException(&e, "scanner");
              LOG_ERROR << "Offset/Size: "+std::to_string(buffer.getOffset())+"/"+std::to_string(file_size);
              break;
            }
          }  //end while loop

          if (posteri.getStateMap().empty()) {
            LOG_DEBUG << "End with no chainstate.";
          } else {
            LOG_DEBUG << "End chainstate: " + WriteChainStateMap(posteri.getStateMap());
          }

          uint64_t duration = blocktime-start_time;
          if (duration != 0 && start_time != 0) {
            LOG_INFO << file_name << " overall rate: "+std::to_string(file_txs*1000/duration)+" txs/sec";
          }
          LOG_INFO << file_name << " has " << std::to_string(file_txs)
                  << " txs, " +std::to_string(file_tfer)+" tfers in "+std::to_string(file_blocks)+" blocks.";
          LOG_INFO << file_name + " coin volume is "+std::to_string(volume);

          block_counter += file_blocks;
          tx_counter += file_txs;
          tfer_count += file_tfer;
          total_volume += volume;
        }  //end file loop
      }  //endif not empty segment
    }  //end segment loop
    LOG_INFO << "Chain has "+std::to_string(tx_counter)+" txs, "
      +std::to_string(tfer_count)+" tfers in "+std::to_string(block_counter)+" blocks.";
    LOG_INFO << "Grand total coin volume is "+std::to_string(total_volume);

    if (!options->write_file.empty()) {
      std::ofstream out_file(options->write_file, std::ios::out);
      if (out_file.is_open()) {
        std::stringstream json_tot;
        if (options->address_filter.size() > 0) {
          auto txs_tree = filter_by_address(block_array, options->address_filter);
          bpt::write_json(json_tot, txs_tree);
        } else {
          bpt::ptree block_ptree;
          block_ptree.add_child("blocks", block_array);
          bpt::write_json(json_tot, block_ptree);
        }
        out_file << json_tot.str();
        out_file.close();
      } else {
        LOG_FATAL << "Failed to open output file '" << options->write_file << "'.";
        return(false);
      }
    }

    block_viewer.dump(std::cout);

    return(true);
  } CASH_CATCH (...) {
    std::exception_ptr p = std::current_exception();
    std::string err("");
    err += (p ? p.__cxa_exception_type()->name() : "null");
    LOG_FATAL << "Error: "+err <<  std::endl;
    std::cerr << err << std::endl;
    return(false);
  }
}

std::unique_ptr<struct scanner_options> ParseScannerOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  auto options = std::make_unique<scanner_options>();

  try {
    po::options_description desc("\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
A block scanner.\n\
Use T1 to scan a tier 1 chain, T2 to scan a T2 chain, and scan to\n\
scan raw transactions.\n\
\n\
Required parameters");
    desc.add_options()
        ("mode", po::value<std::string>(), "Devv mode (T1|T2|scan)")
        ("working-dir", po::value<std::string>(), "Directory where inputs are read and outputs are written")
        ("output", po::value<std::string>(), "Output file (JSON)")
        ;

    po::options_description d2("Optional parameters");
    d2.add_options()
        ("help", "produce help message")
        ("debug-mode", po::value<std::string>(), "Debug mode (on|off|perf) for testing")
        ("expect-version", "look for this block version while scanning")
        ("filter-by-address", po::value<std::string>(), "Filter results by address - only transactions involving this address will written to the JSON output file")
        ;
    desc.add(d2);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << "\n";
      return nullptr;
    }

    if (vm.count("mode")) {
      std::string mode = vm["mode"].as<std::string>();
      if (mode == "SCAN") {
        options->mode = scan;
      } else if (mode == "T1") {
        options->mode = T1;
      } else if (mode == "T2") {
        options->mode = T2;
      } else {
        LOG_WARNING << "unknown mode: " << mode;
      }
      LOG_INFO << "mode: " << options->mode;
    } else {
      LOG_INFO << "mode was not set.";
    }

    if (vm.count("debug-mode")) {
      std::string debug_mode = vm["debug-mode"].as<std::string>();
      if (debug_mode == "on") {
        options->debug_mode = on;
      } else if (debug_mode == "toy") {
        options->debug_mode = toy;
      } else if (debug_mode == "perf") {
        options->debug_mode = perf;
      } else {
        options->debug_mode = off;
      }
      LOG_INFO << "debug_mode: " << options->debug_mode;
    } else {
      LOG_INFO << "debug_mode was not set.";
    }

    if (vm.count("working-dir")) {
      options->working_dir = vm["working-dir"].as<std::string>();
      LOG_INFO << "Working dir: " << options->working_dir;
    } else {
      LOG_INFO << "Working dir was not set.";
    }

    if (vm.count("filter-by-address")) {
      options->address_filter = vm["filter-by-address"].as<std::string>();
      LOG_INFO << "Address filter: " << options->address_filter;
    } else {
      LOG_INFO << "Address filter was not specified.";
    }

    if (vm.count("output")) {
      options->write_file = vm["output"].as<std::string>();
      LOG_INFO << "Output file: " << options->write_file;
    } else {
      LOG_INFO << "Output file was not set.";
    }

    if (vm.count("expect-version")) {
      options->version = vm["expect-version"].as<unsigned int>();
      LOG_INFO << "Expect Block Version: " << options->version;
    } else {
      LOG_INFO << "Expect Block Version was not set, defaulting to 0";
      options->version = 0;
    }
  }
  catch (std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
