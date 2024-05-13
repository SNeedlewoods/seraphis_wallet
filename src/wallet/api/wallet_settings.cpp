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
#include "cryptonote_config.h"

//third party headers
#include "string_tools.h"

//standard headers


#define DEFAULT_INACTIVITY_LOCK_TIMEOUT 90 // a minute and a half

#define SUBADDRESS_LOOKAHEAD_MAJOR 50
#define SUBADDRESS_LOOKAHEAD_MINOR 200


namespace Monero
{

//-------------------------------------------------------------------------------------------------------------------
WalletSettings::WalletSettings(NetworkType nettype, uint64_t kdf_rounds) :
    m_ask_password(AskPasswordType::AskPasswordToDecrypt),
    m_setup_background_mining(BackgroundMiningSetupType::BackgroundMiningMaybe),
    m_always_confirm_transfers(true),
    m_auto_low_priority(true),
    m_auto_refresh(true),
    m_confirm_backlog(true),
    m_confirm_export_overwrite(true),
    m_ignore_fractional_outputs(true),
    m_key_reuse_mitigation2(true),
    m_load_deprecated_formats(false),
    m_merge_destinations(false),
    m_print_ring_members(false),
    m_show_wallet_name_when_locked(false),
    m_segregate_pre_fork_outputs(true),
    m_store_tx_info(true),
    m_track_uses(false),
    m_unattended(false),
    m_watch_only(false),
    m_key_device_type(hw::device::device_type::SOFTWARE),
    m_nettype(nettype),
    m_export_format(ExportFormat::Binary),
    m_refresh_type(RefreshType::RefreshOptimizeCoinbase),
    m_subaddress_lookahead_major(SUBADDRESS_LOOKAHEAD_MAJOR),
    m_subaddress_lookahead_minor(SUBADDRESS_LOOKAHEAD_MINOR),
    m_confirm_backlog_threshold(0),
    m_default_priority(0),
    m_inactivity_lock_timeout(DEFAULT_INACTIVITY_LOCK_TIMEOUT),
    // TODO : why not set to DEFAULT_MIN_OUTPUT_COUNT, then we can skip setting to DEFAULT_MIN_OUTPUT_COUNT when m_min_output_count == 0 in wallet2::create_transactions_2()
    m_min_output_count(0),
    m_ignore_outputs_above(MONEY_SUPPLY),
    m_ignore_outputs_below(0),
    m_kdf_rounds(kdf_rounds),
    m_max_reorg_depth(ORPHANED_BLOCKS_MAX_COUNT),
    // TODO : why not set to DEFAULT_MIN_OUTPUT_VALUE, then we can skip setting to DEFAULT_MIN_OUTPUT_VALUE when m_min_output_value == 0 in wallet2::create_transactions_2()
    m_min_output_value(0),
    m_refresh_from_block_height(0),
    m_segregation_height(0),
    m_skip_to_height(0)
{
}
//-------------------------------------------------------------------------------------------------------------------
void WalletSettings::clear()
{
//  m_blockchain.clear();
//  m_transfers.clear();
//  m_key_images.clear();
//  m_pub_keys.clear();
//  m_unconfirmed_txs.clear();
//  m_payments.clear();
//  m_tx_keys.clear();
//  m_additional_tx_keys.clear();
//  m_confirmed_txs.clear();
//  m_unconfirmed_payments.clear();
//  m_scanned_pool_txs[0].clear();
//  m_scanned_pool_txs[1].clear();
//  m_address_book.clear();
//  m_subaddresses.clear();
//  m_subaddress_labels.clear();
//  m_multisig_rounds_passed = 0;
//  m_device_last_key_image_sync = 0;
//  m_pool_info_query_time = 0;
  m_skip_to_height = 0;
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
