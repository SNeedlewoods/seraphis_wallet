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

class WalletSettings
{
public:
    WalletSettings(NetworkType nettype, uint64_t kdf_rounds);
    void clear();
    void prepare_file_names(const std::string &file_path);

private:
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
    bool m_ignore_fractional_outputs;
    bool m_key_reuse_mitigation2;
    bool m_load_deprecated_formats;
    bool m_merge_destinations;
    bool m_print_ring_members;
    bool m_show_wallet_name_when_locked;
    bool m_segregate_pre_fork_outputs;
    bool m_store_tx_info; /* request txkey to be returned in RPC, and store in the wallet cache file */
    bool m_track_uses;
    bool m_unattended;
    bool m_watch_only;

    ExportFormat m_export_format;

    hw::device::device_type m_key_device_type;

    NetworkType m_nettype;

    RefreshType m_refresh_type;

    std::size_t m_subaddress_lookahead_major;
    std::size_t m_subaddress_lookahead_minor;

    std::string m_device_derivation_path;
    std::string m_device_name;
    std::string m_keys_file;
    std::string m_seed_language;
    std::string m_wallet_file;

    std::uint32_t m_confirm_backlog_threshold;
    std::uint32_t m_default_priority;
    std::uint32_t m_inactivity_lock_timeout;
    std::uint32_t m_min_output_count;
    std::uint64_t m_ignore_outputs_above;
    std::uint64_t m_ignore_outputs_below;
    std::uint64_t m_kdf_rounds;
    std::uint64_t m_max_reorg_depth;
    std::uint64_t m_min_output_value;
    std::uint64_t m_refresh_from_block_height;
    std::uint64_t m_segregation_height;
    // m_skip_to_height is useful when we don't want to modify the wallet's restore height.
    // m_refresh_from_block_height is also a wallet's restore height which should remain constant unless explicitly modified by the user.
    std::uint64_t m_skip_to_height;
};


} // namespace
