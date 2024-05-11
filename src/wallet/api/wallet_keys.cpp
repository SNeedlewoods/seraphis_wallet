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

//paired header
#include "wallet_keys.h"

//local headers

//third party headers

//standard headers


namespace Monero
{

WalletKeys::WalletKeys()
{
}
//-------------------------------------------------------------------------------------------------------------------
void WalletKeys::setup_keys(const epee::wipeable_string &password,
                            const std::unique_ptr<WalletSettings> &wallet_settings,
                            cryptonote::account_base &account)
{
    crypto::chacha_key key{};
    crypto::generate_chacha_key(password.data(), password.size(), key, wallet_settings->m_kdf_rounds);

    // re-encrypt, but keep viewkey unencrypted
    if (wallet_settings->m_ask_password == AskPasswordToDecrypt &&
       !wallet_settings->m_unattended &&
       !wallet_settings->m_watch_only)
    {
        account.encrypt_keys(key);
        account.decrypt_viewkey(key);
    }

    m_cache_key = Utils::derive_cache_key(key);

    //  get_ringdb_key();
}
//-------------------------------------------------------------------------------------------------------------------
//crypto::chacha_key wallet2::get_ringdb_key()
//{
//  if (!m_ringdb_key)
//  {
//    MINFO("caching ringdb key");
//    crypto::chacha_key key;
//    generate_chacha_key_from_secret_keys(key);
//    m_ringdb_key = key;
//  }
//  return *m_ringdb_key;
//}


} // namespace
