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

////
// WalletKeys
// - housing for critical data like password and keys
///
class WalletKeys
{
public:
    WalletKeys();

    /**
    * brief: get_ringdb_key - generates/returns chacha key for ringdb
    * param: account -
    * param: kdf_rounds -
    * return: m_ringdb_key
    */
    crypto::chacha_key get_ringdb_key(cryptonote::account_base &account, const std::uint64_t &kdf_rounds);
    /**
    * brief: setup_keys - generates chacha key from password and sets m_cache_key
    * param: password -
    * param: wallet_settings -
    * param: account -
    */
    void setup_keys(const epee::wipeable_string &password, const std::unique_ptr<WalletSettings> &wallet_settings, cryptonote::account_base &account);

private:
    friend class WalletImpl;

    crypto::chacha_key m_cache_key;
    boost::optional<crypto::chacha_key> m_ringdb_key;
};


} // namespace
