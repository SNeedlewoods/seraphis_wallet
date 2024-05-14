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
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/subaddress_index.h"
#include "crypto/crypto.h"
#include "device/device.hpp"
#include "wallet2_api.h"

//third party headers
#include "string_tools.h"

//standard headers
#include <cstdint>
#include <string>
#include <vector>


namespace Monero
{

enum AskPasswordType
{
    AskPasswordNever = 0,
    AskPasswordOnAction = 1,
    AskPasswordToDecrypt = 2,
};

enum BackgroundMiningSetupType {
    BackgroundMiningMaybe = 0,
    BackgroundMiningYes = 1,
    BackgroundMiningNo = 2,
};

enum ExportFormat {
    Binary = 0,
    Ascii,
};

enum RefreshType {
    RefreshFull,
    RefreshOptimizeCoinbase,
    RefreshNoCoinbase,
    RefreshDefault = RefreshOptimizeCoinbase,
};

class hashchain
{
public:
    hashchain(): m_genesis(crypto::null_hash), m_offset(0) {}

    size_t size() const { return m_blockchain.size() + m_offset; }
    size_t offset() const { return m_offset; }
    const crypto::hash &genesis() const { return m_genesis; }
    void push_back(const crypto::hash &hash) { if (m_offset == 0 && m_blockchain.empty()) m_genesis = hash; m_blockchain.push_back(hash); }
    bool is_in_bounds(size_t idx) const { return idx >= m_offset && idx < size(); }
    const crypto::hash &operator[](size_t idx) const { return m_blockchain[idx - m_offset]; }
    crypto::hash &operator[](size_t idx) { return m_blockchain[idx - m_offset]; }
    void crop(size_t height) { m_blockchain.resize(height - m_offset); }
    void clear() { m_offset = 0; m_blockchain.clear(); }
    bool empty() const { return m_blockchain.empty() && m_offset == 0; }
    void trim(size_t height) { while (height > m_offset && m_blockchain.size() > 1) { m_blockchain.pop_front(); ++m_offset; } m_blockchain.shrink_to_fit(); }
    void refill(const crypto::hash &hash) { m_blockchain.push_back(hash); --m_offset; }

    template <class t_archive>
        inline void serialize(t_archive &a, const unsigned int ver)
        {
            a & m_offset;
            a & m_genesis;
            a & m_blockchain;
        }

    BEGIN_SERIALIZE_OBJECT()
    VERSION_FIELD(0)
    VARINT_FIELD(m_offset)
    FIELD(m_genesis)
    FIELD(m_blockchain)
    END_SERIALIZE()

private:
    size_t m_offset;
    crypto::hash m_genesis;
    std::deque<crypto::hash> m_blockchain;
};

class WalletSettings
{
public:
    WalletSettings(NetworkType nettype, uint64_t kdf_rounds);
    /**
    * brief: clear -
    */
    void clear();
    /**
    * brief: init_type -
    * param: device_type -
    * param: account -
    * param: account_public_address -
    */
    void init_type(hw::device::device_type device_type, cryptonote::account_base &account, cryptonote::account_public_address &account_public_address);
    /**
    * brief: prepare_file_names -
    * param: file_path -
    */
    void prepare_file_names(const std::string &file_path);
    /**
    * brief: setup_new_blockchain -
    */
    void setup_new_blockchain();

private:
    friend class SubaddressImpl;
    friend class WalletImpl;
    friend struct Wallet2CallbackImpl;
    friend class WalletKeys;

    AskPasswordType m_ask_password;

    BackgroundMiningSetupType m_setup_background_mining;

    bool m_always_confirm_transfers;
    bool m_auto_low_priority;
    bool m_auto_refresh;
    bool m_confirm_backlog;
    bool m_confirm_export_overwrite;
    bool m_enable_multisig;
    bool m_ignore_fractional_outputs;
    bool m_key_reuse_mitigation2;
    bool m_load_deprecated_formats;
    bool m_merge_destinations;
    bool m_multisig;
    bool m_original_keys_available;
    bool m_print_ring_members;
    bool m_show_wallet_name_when_locked;
    bool m_segregate_pre_fork_outputs;
    bool m_store_tx_info; /* request txkey to be returned in RPC, and store in the wallet cache file */
    bool m_track_uses;
    bool m_unattended;
    bool m_watch_only;

    crypto::secret_key m_original_view_secret_key;
    cryptonote::account_public_address m_original_address;

    ExportFormat m_export_format;

    hashchain m_blockchain;

    hw::device::device_type m_key_device_type;

    NetworkType m_nettype;

    RefreshType m_refresh_type;

    std::size_t m_subaddress_lookahead_major;
    std::size_t m_subaddress_lookahead_minor;

    std::string m_device_derivation_path;
    std::string m_device_name;
    std::string m_keys_file;
    std::string m_mms_file;
    std::string m_seed_language;
    std::string m_wallet_file;

    std::uint32_t m_confirm_backlog_threshold;
    std::uint32_t m_default_mixin;
    std::uint32_t m_default_priority;
    std::uint32_t m_inactivity_lock_timeout;
    std::uint32_t m_min_output_count;
    std::uint32_t m_multisig_rounds_passed;
    std::uint32_t m_multisig_threshold;

    std::uint64_t m_ignore_outputs_above;
    std::uint64_t m_ignore_outputs_below;
    std::uint64_t m_kdf_rounds;
    std::uint64_t m_last_block_reward;
    std::uint64_t m_max_reorg_depth;
    std::uint64_t m_min_output_value;
    std::uint64_t m_refresh_from_block_height;
    std::uint64_t m_segregation_height;
    // m_skip_to_height is useful when we don't want to modify the wallet's restore height.
    // m_refresh_from_block_height is also a wallet's restore height which should remain constant unless explicitly modified by the user.
    std::uint64_t m_skip_to_height;

    std::unordered_map<crypto::public_key, cryptonote::subaddress_index> m_subaddresses;

    std::vector<crypto::public_key> m_multisig_derivations;
    std::vector<crypto::public_key> m_multisig_signers;
    std::vector<std::vector<std::string>> m_subaddress_labels;
};


} // namespace
