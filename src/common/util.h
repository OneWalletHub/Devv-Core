/*
 * util.h general utilities.
 *
 * @copywrite  2018 Devvio Inc
 */

/**
 * Server/client environment: argument handling, config file parsing,
 * logging, thread wrappers, startup time
 */
#ifndef DEVV_UTIL_H
#define DEVV_UTIL_H

#include <atomic>
#include <chrono>
#include <exception>
#include <map>
#include <bitset>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <thread>

#include "common/devv_types.h"
#include "common/devv_constants.h"
#include "common/logger.h"
#include "common/minitrace.h"

namespace Devv
{

template <typename Container>
void CheckSizeEqual(const Container& c, size_t size) {
  if (c.size() != size) {
    std::string err = "Container must be "
                      + std::to_string(size) + " bytes (" + std::to_string(c.size()) + ")";
    throw std::runtime_error(err);
  }
}

/**
 * Get the number of milliseconds since the Epoch (Jan 1, 1970)
 * @return Number of milliseconds
 */
static uint64_t GetMillisecondsSinceEpoch() {
  //MTR_SCOPE_FUNC();
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch());
  return ms.count();
}

/** Setup the runtime environment. */
void SetupEnvironment();

/** Setup networking.
 *  @return true iff networking setup successfully
 *  @return false if an error may have occurred
*/
bool SetupNetworking();

/** Formats an exception for logging.
 *  @param pex pointer to the exception
 *  @param pszThread name of the thread/module where exception occurred
 *  @return a formatted string to log the exception
*/
std::string FormatException(const std::exception* pex,
  const std::string pszThread);

/** Sets the path for config files, inputs, and outputs
 *  @param path string for the working path
*/
void setWorkPath(std::string path);

/** Gets the path for config files, inputs, and outputs
 *  @return path string for the working path
*/
std::string getWorkPath();

/** Syncs a file
 *  @param file pointer to the FILE
*/
void FileCommit(FILE* file);

/** Left trims chars from a string.
 *  @note the string is modified by reference
 *  @param s the string to trim
 *  @param t pointer to chars that should be trimmed
*/
inline void ltrim(std::string& s, const char* t) {
  s.erase(0, s.find_first_not_of(t));
}

/** Right trims chars from a string.
 *  @note the string is modified by reference
 *  @param s the string to trim
 *  @param t pointer to chars that should be trimmed
*/
inline void rtrim(std::string& s, const char* t) {
  s.erase(s.find_last_not_of(t) +1);
}

/** Trims whitespace chars from both sides of a string.
 *  @note the string is modified by reference
 *  @param s the string to trim
*/
inline std::string trim(std::string& s) {
  ltrim(s, " \t\n\r\f\v");
  rtrim(s, " \t\n\r\f\v");
  return(s);
}

/** Attempts to raise the file descriptor limit.
 * This function tries to raise the file descriptor limit to the requested number.
 * It returns the actual file descriptor limit (more or less than nMinFD)
 *  @param nMinFD the target file descriptor limit
 *  @return the actual file descriptor limit
*/
int RaiseFileDescriptorLimit(int nMinFD);

/**
 * Tries to allocate disk space for a file
 * This function is advisory, not a guarantee.
 * The range specified in the arguments will never contain live data.
 * @param file pointer to the file
 * @param offset an allocation offset
 * @param length length of the requested allocation
 */
void AllocateFileRange(FILE* file, unsigned int offset, unsigned int length);

/**
 * Tries to write a string to a stream
 * @param str the string to write
 * @param fout pointer to the stream to write
 * @return number of bytes written
 */
int StreamWriteStr(const std::string& str, std::ostream* fout);

/**
 * Reads a file into a string
 * @param file_path the path of the file
 * @return if the file is readable, returns a string of its contents
 * @return if file is not readable, returns an empty string
 */
std::string ReadFile(const std::string& file_path);

/**
 * Reacts to an OS signal.
 * Currently shuts down on any signal.
 * @param signum the signal
 */
void signalHandler(int signum);

/**
 * Checks for switch char
 * @param c the character to check
 * @return true iff char is a switch in this environment
 * @return false otherwise
 */
inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/** Maps a hex digit to an int value.
 *  @param hex digit to get the int value for
 *  @return int value of this hex digit
*/
static int char2int(char in) {
  if (in >= '0' && in <= '9') {
    return(in - '0');
  }
  if (in >= 'A' && in <= 'F') {
    return(in - 'A' + 10);
  }
  if (in >= 'a' && in <= 'f') {
  return(in - 'a' + 10);
  }
  return(-1);
}

static bool SearchString(const std::string & strHaystack, const std::string & strNeedle
    , bool case_sensitive)
{
  if (case_sensitive) {
    return (strHaystack.find(strNeedle) != std::string::npos);
  } else {
    auto it = std::search(
      strHaystack.begin(), strHaystack.end(),
      strNeedle.begin(),   strNeedle.end(),
      [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );
    return (it != strHaystack.end());
  }
}

/**
 * Timer. A simple timer class.
 */
class Timer
{
public:
  Timer() { clock_gettime(CLOCK_REALTIME, &beg_); }

  double operator()() { return elapsed(); }

  double elapsed() {
    clock_gettime(CLOCK_REALTIME, &end_);
    return (end_.tv_sec - beg_.tv_sec)*1000 +
      (end_.tv_nsec - beg_.tv_nsec) / 1000000.;
  }

  void reset() { clock_gettime(CLOCK_REALTIME, &beg_); }

private:
  timespec beg_, end_;
};

/**
 * Threadpool. A simple ThreadPool class.
 */
class ThreadPool {

public:

  template<typename Index, typename Callable>
  static void ParallelFor(Index start, Index end, Callable func, unsigned n_cpu_factor = 1) {
    // Estimate number of threads in the pool
    const static unsigned nb_threads_hint = unsigned (std::thread::hardware_concurrency()/n_cpu_factor);
    const static unsigned nb_threads = (nb_threads_hint == 0u ? 8u : nb_threads_hint);

    // Size of a slice for the range functions
    Index n = end - start + 1;
    Index slice = (Index) std::round(n / static_cast<double> (nb_threads));
    slice = std::max(slice, Index(1));

    // [Helper] Inner loop
    auto launchRange = [&func] (int k1, int k2) {
      for (Index k = k1; k < k2; k++) {
        func(k);
      }
    };

    // Create pool and launch jobs
    std::vector<std::thread> pool;
    pool.reserve(nb_threads);
    Index i1 = start;
    Index i2 = std::min(start + slice, end);
    for (unsigned i = 0; i + 1 < nb_threads && i1 < end; ++i) {
      pool.emplace_back(launchRange, i1, i2);
      i1 = i2;
      i2 = std::min(i2 + slice, end);
    }
    if (i1 < end) {
      pool.emplace_back(launchRange, i1, end);
    }

    // Wait for jobs to finish
    for (std::thread &t : pool) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  // Serial version for easy comparison
  template<typename Index, typename Callable>
  static void SequentialFor(Index start, Index end, Callable func) {
    for (Index i = start; i < end; i++) {
      func(i);
    }
  }

};

} //end namespace Devv

#endif // DEVV_UTIL_H
