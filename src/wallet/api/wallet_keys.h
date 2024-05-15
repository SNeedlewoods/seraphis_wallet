// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

// Handle keys for the wallet API

#pragma once

//local headers
#include "common/util.h"
#include "cryptonote_basic/account.h"
#include "crypto/chacha.h"
#include "wallet2_api.h"
#include "wallet_settings.h"

//third party headers
#include "wipeable_string.h"
#include <boost/optional/optional.hpp>

//standard headers

//forward declarations


namespace Monero
{

class WalletSettings;

////
// WalletKeys
// - housing for critical data like password and keys
///
class WalletKeys
{
public:
    struct keys_file_data
    {
      crypto::chacha_iv iv;
      std::string account_data;

      BEGIN_SERIALIZE_OBJECT()
      FIELD(iv)
      FIELD(account_data)
      END_SERIALIZE()
    };

    WalletKeys();

    /**
    * brief: create_keys_file -
    * param: TODO -
    */
    void create_keys_file(const std::string &wallet_, bool watch_only, const epee::wipeable_string &password, bool create_address_file, cryptonote::account_base &account, const std::unique_ptr<WalletSettings> &wallet_settings);
    /**
    * brief: get_keys_file_data -
    * param: password - password of wallet file
    * param: watch_only -
    * param: account -
    * param: kdf_rounds -
    * return: TODO -
    */
    boost::optional<WalletKeys::keys_file_data> get_keys_file_data(const epee::wipeable_string &password, bool watch_only, cryptonote::account_base &account, const std::unique_ptr<WalletSettings> &wallet_settings);
    /**
    * brief: get_ringdb_key - generates/returns chacha key for ringdb
    * param: account -
    * param: kdf_rounds -
    * return: m_ringdb_key
    */
    crypto::chacha_key get_ringdb_key(cryptonote::account_base &account, const std::uint64_t kdf_rounds);
    /**
    * brief: lock_keys_file -
    * param: wallet_file -
    * param: keys_file -
    * return:
    */
    bool lock_keys_file(std::string wallet_file, std::string keys_file);
    /**
    * brief: setup_keys - generates chacha key from password and sets m_cache_key
    * param: password -
    * param: wallet_settings -
    * param: account -
    */
    void setup_keys(const epee::wipeable_string &password, const std::unique_ptr<WalletSettings> &wallet_settings, cryptonote::account_base &account);
    /**
    * brief: store_keys - stores wallet information to wallet file
    * param: keys_file_name - name of wallet file
    * param: password - password of wallet file
    * param: watch_only - true to save only view key, false to save both spend and view keys
    * param: account -
    * param: kdf_rounds -
    * return: true if succeeded
    */
    bool store_keys(const std::string &keys_file_name, const epee::wipeable_string &password, bool watch_only, cryptonote::account_base &account, const std::unique_ptr<WalletSettings> &wallet_settings);
    /**
    * brief: unlock_keys_file -
    * param: wallet_file -
    * param: keys_file -
    * return:
    */
    bool unlock_keys_file(std::string wallet_file, std::string keys_file);

private:
    friend class WalletImpl;

    boost::optional<crypto::chacha_key> m_ringdb_key;

    crypto::chacha_key m_cache_key;

    std::unique_ptr<tools::file_locker> m_keys_file_locker;
};


} // namespace
