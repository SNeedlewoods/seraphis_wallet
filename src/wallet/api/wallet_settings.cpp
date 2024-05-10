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
#include "wallet_settings.h"

//local headers

//third party headers
#include "string_tools.h"

//standard headers


namespace Monero
{

//-------------------------------------------------------------------------------------------------------------------
WalletSettings::WalletSettings(NetworkType nettype, uint64_t kdf_rounds)
    : m_nettype(nettype),
    m_kdf_rounds(kdf_rounds),
    m_refresh_from_block_height(0),
    m_ask_password(AskPasswordToDecrypt),
    m_watch_only(false),
    m_unattended(false),
    m_key_device_type(hw::device::device_type::SOFTWARE)
{
}
//-------------------------------------------------------------------------------------------------------------------
void WalletSettings::prepare_file_names(const std::string& file_path)
{
    m_keys_file = file_path;
    m_wallet_file = file_path;
    if (epee::string_tools::get_extension(m_keys_file) == "keys")
    { //provided keys file name
        m_wallet_file = epee::string_tools::cut_off_extension(m_wallet_file);
    } else
    { //provided wallet file name
        m_keys_file += ".keys";
    }
//    m_mms_file = file_path + ".mms";
}


} // namespace
