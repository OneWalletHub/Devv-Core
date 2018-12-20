/*
 * logger.cpp implements logger
 *
 * @copywrite  2018 Devvio Inc
 */

#include "common/logger.h"

// This seems to be needed to prevent undefined references
// to boost::log functions while linking. Probably a better way to
// do it than this (mckenney)
BOOST_LOG_GLOBAL_LOGGER_INIT(logger, src::severity_logger_mt) {
  logger_t logger;
  return logger;
}

void init_log() {
  LoggerContext context;
  init_log(context);
  LOG_INFO << "Git Version: " << "git-" << Devv::Version::GIT_SHA1
           << " : " << Devv::Version::GIT_BRANCH
           << " : " << Devv::Version::GIT_DATE
           << " : " << Devv::Version::GIT_COMMIT_SUBJECT;
}
