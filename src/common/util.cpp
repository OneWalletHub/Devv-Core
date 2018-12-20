/*
 * util.cpp general utilities.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "util.h"

#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <locale>
#include <unistd.h>
#include <chrono>

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifndef WIN32
// for posix_fallocate
#ifdef __linux__

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L

#endif // __linux__

#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/conf.h>

#include "logger.h"

namespace Devv
{

std::string ReadFile(const std::string& file_path)
{
  MTR_SCOPE_FUNC();
  std::ifstream file(file_path);
  if (!file.good()) {
    LOG_ERROR << "File "+file_path+" could not be found";
  }

  if (!file.is_open()) {
    LOG_ERROR << "File "+file_path+" could not be opened, check permissions";
  }

  std::string output("");
  for(std::string line; std::getline(file, line);) {
    output += line+"\n";
  }
  return(output);
}

std::string FormatException(const std::exception* pex, const std::string pszThread)
{
  MTR_SCOPE_FUNC();
  std::string msg = "";
  if (pex) {
    msg += "EXCEPTION[";
    msg += pszThread;
    msg += "]";
    msg += ": ";
    msg += std::string(pex->what());
  } else {
    msg += "UNKNOWN EXCEPTION[";
    msg += pszThread;
    msg += "]";
  }
  msg += "\n";
  return(msg);
}

void signalHandler(int signum) {
  //StartShutdown();
  LOG_INFO << "Signal ("+std::to_string(signum)+") received.";
  //shutdown();
}

} //end namespace Devv
