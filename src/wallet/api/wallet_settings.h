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

// Handle settings for the wallet API

#pragma once

//local headers
#include "device/device.hpp"
#include "wallet2_api.h"

//third party headers
#include "string_tools.h"

//standard headers
#include <cstdint>
#include <string>


namespace Monero
{

enum AskPasswordType
{
    AskPasswordNever = 0,
    AskPasswordOnAction = 1,
    AskPasswordToDecrypt = 2,
};

class WalletSettings
{
public:
    WalletSettings(NetworkType nettype, uint64_t kdf_rounds);
    void prepare_file_names(const std::string &file_path);

private:
    friend class WalletImpl;
    friend struct Wallet2CallbackImpl;
    friend class WalletKeys;

    NetworkType m_nettype;
    std::uint64_t m_kdf_rounds;
    std::uint64_t m_refresh_from_block_height;
    std::string m_seed_language;
    std::string m_wallet_file;
    std::string m_keys_file;
//    std::string m_mms_file;
    AskPasswordType m_ask_password;
    bool m_watch_only;
    bool m_unattended;
    hw::device::device_type m_key_device_type;
};


} // namespace
