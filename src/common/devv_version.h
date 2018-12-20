/*
 * devv_version.h
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once
#include <string>

namespace Devv
{

struct Version
{
  static const std::string GIT_SHA1;
  static const std::string GIT_DATE;
  static const std::string GIT_COMMIT_SUBJECT;
  static const std::string GIT_BRANCH;
};

}
