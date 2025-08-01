// addressbook.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// falaclient is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace sqlite
{
  class database;
}

class AddressBook
{
public:
  static void Init(const bool p_AddressBookEncrypt, const std::string& p_Pass);
  static void Cleanup();

  static bool ChangePass(const bool p_CacheEncrypt,
                         const std::string& p_OldPass, const std::string& p_NewPass);

  static void Add(const std::string& p_MsgId, const std::set<std::string>& p_Addresses);
  static void AddFrom(const std::string& p_Address);
  static std::vector<std::string> Get(const std::string& p_Filter);
  static std::vector<std::string> GetFrom(const std::string& p_Filter);

private:
  static void InitCacheDir();
  static std::string GetAddressBookCacheDir();
  static std::string GetAddressBookCacheDbDir();
  static std::string GetAddressBookTempDbDir();

private:
  static std::mutex m_Mutex;
  static bool m_AddressBookEncrypt;
  static std::string m_Pass;
  static std::unique_ptr<sqlite::database> m_Db;
  static bool m_Dirty;
};
