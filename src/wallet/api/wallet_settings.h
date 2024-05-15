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
#include "net/http.h"
#include "net/http_client.h"
#include "wallet/api/wallet2_api.h"
#include "wallet/message_store.h"
#include "wallet/wallet_errors.h"

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
    struct cache_file_data
    {
        crypto::chacha_iv iv;
        std::string cache_data;

        BEGIN_SERIALIZE_OBJECT()
        FIELD(iv)
        FIELD(cache_data)
        END_SERIALIZE()
    };
    struct address_book_row
    {
        cryptonote::account_public_address m_address;
        crypto::hash8 m_payment_id;
        std::string m_description;
        bool m_is_subaddress;
        bool m_has_payment_id;

        BEGIN_SERIALIZE_OBJECT()
        VERSION_FIELD(0)
        FIELD(m_address)
        FIELD(m_payment_id)
        FIELD(m_description)
        FIELD(m_is_subaddress)
        FIELD(m_has_payment_id)
        END_SERIALIZE()
    };
    struct multisig_info
    {
        struct LR
        {
            rct::key m_L;
            rct::key m_R;

            BEGIN_SERIALIZE_OBJECT()
            FIELD(m_L)
            FIELD(m_R)
            END_SERIALIZE()
        };

        crypto::public_key m_signer;
        std::vector<LR> m_LR;
        std::vector<crypto::key_image> m_partial_key_images; // one per key the participant has

        BEGIN_SERIALIZE_OBJECT()
        FIELD(m_signer)
        FIELD(m_LR)
        FIELD(m_partial_key_images)
        END_SERIALIZE()
    };
    struct transfer_details
    {
        uint64_t m_block_height;
        cryptonote::transaction_prefix m_tx;
        crypto::hash m_txid;
        uint64_t m_internal_output_index;
        uint64_t m_global_output_index;
        bool m_spent;
        bool m_frozen;
        uint64_t m_spent_height;
        crypto::key_image m_key_image; //TODO: key_image stored twice :(
        rct::key m_mask;
        uint64_t m_amount;
        bool m_rct;
        bool m_key_image_known;
        bool m_key_image_request; // view wallets: we want to request it; cold wallets: it was requested
        uint64_t m_pk_index;
        cryptonote::subaddress_index m_subaddr_index;
        bool m_key_image_partial;
        std::vector<rct::key> m_multisig_k;
        std::vector<multisig_info> m_multisig_info; // one per other participant
        std::vector<std::pair<uint64_t, crypto::hash>> m_uses;

        bool is_rct() const { return m_rct; }
        uint64_t amount() const { return m_amount; }
        const crypto::public_key get_public_key() const {
            crypto::public_key output_public_key;
            THROW_WALLET_EXCEPTION_IF(m_tx.vout.size() <= m_internal_output_index,
                    tools::error::wallet_internal_error, "Too few outputs, outputs may be corrupted");
            THROW_WALLET_EXCEPTION_IF(!get_output_public_key(m_tx.vout[m_internal_output_index], output_public_key),
                    tools::error::wallet_internal_error, "Unable to get output public key from output");
            return output_public_key;
        };

        BEGIN_SERIALIZE_OBJECT()
        FIELD(m_block_height)
        FIELD(m_tx)
        FIELD(m_txid)
        FIELD(m_internal_output_index)
        FIELD(m_global_output_index)
        FIELD(m_spent)
        FIELD(m_frozen)
        FIELD(m_spent_height)
        FIELD(m_key_image)
        FIELD(m_mask)
        FIELD(m_amount)
        FIELD(m_rct)
        FIELD(m_key_image_known)
        FIELD(m_key_image_request)
        FIELD(m_pk_index)
        FIELD(m_subaddr_index)
        FIELD(m_key_image_partial)
        FIELD(m_multisig_k)
        FIELD(m_multisig_info)
        FIELD(m_uses)
        END_SERIALIZE()
    };
    typedef std::vector<uint64_t> amounts_container;
    struct payment_details
    {
        crypto::hash m_tx_hash;
        uint64_t m_amount;
        amounts_container m_amounts;
        uint64_t m_fee;
        uint64_t m_block_height;
        uint64_t m_unlock_time;
        uint64_t m_timestamp;
        bool m_coinbase;
        cryptonote::subaddress_index m_subaddr_index;

        BEGIN_SERIALIZE_OBJECT()
        VERSION_FIELD(0)
        FIELD(m_tx_hash)
        VARINT_FIELD(m_amount)
        FIELD(m_amounts)
        VARINT_FIELD(m_fee)
        VARINT_FIELD(m_block_height)
        VARINT_FIELD(m_unlock_time)
        VARINT_FIELD(m_timestamp)
        FIELD(m_coinbase)
        FIELD(m_subaddr_index)
        END_SERIALIZE()
    };
    struct pool_payment_details
    {
        payment_details m_pd;
        bool m_double_spend_seen;

        BEGIN_SERIALIZE_OBJECT()
        VERSION_FIELD(0)
        FIELD(m_pd)
        FIELD(m_double_spend_seen)
        END_SERIALIZE()
    };
    struct unconfirmed_transfer_details
    {
        cryptonote::transaction_prefix m_tx;
        uint64_t m_amount_in;
        uint64_t m_amount_out;
        uint64_t m_change;
        time_t m_sent_time;
        std::vector<cryptonote::tx_destination_entry> m_dests;
        crypto::hash m_payment_id;
        enum { pending, pending_in_pool, failed } m_state;
        uint64_t m_timestamp;
        uint32_t m_subaddr_account;   // subaddress account of your wallet to be used in this transfer
        std::set<uint32_t> m_subaddr_indices;  // set of address indices used as inputs in this transfer
        std::vector<std::pair<crypto::key_image, std::vector<uint64_t>>> m_rings; // relative

        BEGIN_SERIALIZE_OBJECT()
        VERSION_FIELD(1)
        FIELD(m_tx)
        VARINT_FIELD(m_amount_in)
        VARINT_FIELD(m_amount_out)
        VARINT_FIELD(m_change)
        VARINT_FIELD(m_sent_time)
        FIELD(m_dests)
        FIELD(m_payment_id)
        if (version >= 1)
            VARINT_FIELD(m_state)
        VARINT_FIELD(m_timestamp)
        VARINT_FIELD(m_subaddr_account)
        FIELD(m_subaddr_indices)
        FIELD(m_rings)
        END_SERIALIZE()
    };
    struct confirmed_transfer_details
    {
        cryptonote::transaction_prefix m_tx;
        uint64_t m_amount_in;
        uint64_t m_amount_out;
        uint64_t m_change;
        uint64_t m_block_height;
        std::vector<cryptonote::tx_destination_entry> m_dests;
        crypto::hash m_payment_id;
        uint64_t m_timestamp;
        uint64_t m_unlock_time;
        uint32_t m_subaddr_account;   // subaddress account of your wallet to be used in this transfer
        std::set<uint32_t> m_subaddr_indices;  // set of address indices used as inputs in this transfer
        std::vector<std::pair<crypto::key_image, std::vector<uint64_t>>> m_rings; // relative

        confirmed_transfer_details(): m_amount_in(0), m_amount_out(0), m_change((uint64_t)-1), m_block_height(0), m_payment_id(crypto::null_hash), m_timestamp(0), m_unlock_time(0), m_subaddr_account((uint32_t)-1) {}
        confirmed_transfer_details(const unconfirmed_transfer_details &utd, uint64_t height):
            m_tx(utd.m_tx), m_amount_in(utd.m_amount_in), m_amount_out(utd.m_amount_out), m_change(utd.m_change), m_block_height(height), m_dests(utd.m_dests), m_payment_id(utd.m_payment_id), m_timestamp(utd.m_timestamp), m_unlock_time(utd.m_tx.unlock_time), m_subaddr_account(utd.m_subaddr_account), m_subaddr_indices(utd.m_subaddr_indices), m_rings(utd.m_rings) {}

        BEGIN_SERIALIZE_OBJECT()
        VERSION_FIELD(1)
        if (version >= 1)
            FIELD(m_tx)
        VARINT_FIELD(m_amount_in)
        VARINT_FIELD(m_amount_out)
        VARINT_FIELD(m_change)
        VARINT_FIELD(m_block_height)
        FIELD(m_dests)
        FIELD(m_payment_id)
        VARINT_FIELD(m_timestamp)
        VARINT_FIELD(m_unlock_time)
        VARINT_FIELD(m_subaddr_account)
        FIELD(m_subaddr_indices)
        FIELD(m_rings)
        END_SERIALIZE()
    };


    typedef std::vector<transfer_details> transfer_container;
    typedef std::unordered_multimap<crypto::hash, payment_details> payment_container;

    template <class t_archive>
    inline void serialize(t_archive &a, const unsigned int ver)
    {
        // TODO NOW
        uint64_t dummy_refresh_height = 0; // moved to keys file
        if(ver < 5)
            return;
        if (ver < 19)
        {
            std::vector<crypto::hash> blockchain;
            a & blockchain;
            m_blockchain.clear();
            for (const auto &b: blockchain)
                m_blockchain.push_back(b);
        }
        else
            a & m_blockchain;
        a & m_transfers;
        a & m_account_public_address;
        a & m_key_images;
        if(ver < 6)
            return;
        a & m_unconfirmed_txs;
        if(ver < 7)
            return;
        a & m_payments;
        if(ver < 8)
            return;
        a & m_tx_keys;
        if(ver < 9)
            return;
        a & m_confirmed_txs;
        if(ver < 11)
            return;
        a & dummy_refresh_height;
        if(ver < 12)
            return;
        a & m_tx_notes;
        if(ver < 13)
            return;
        if (ver < 17)
        {
            // we're loading an old version, where m_unconfirmed_payments was a std::map
            std::unordered_map<crypto::hash, payment_details> m;
            a & m;
            m_unconfirmed_payments.clear();
            for (std::unordered_map<crypto::hash, payment_details>::const_iterator i = m.begin(); i != m.end(); ++i)
                m_unconfirmed_payments.insert(std::make_pair(i->first, pool_payment_details{i->second, false}));
        }
        if(ver < 14)
            return;
        if(ver < 15)
        {
            // we're loading an older wallet without a pubkey map, rebuild it
            m_pub_keys.clear();
            for (size_t i = 0; i < m_transfers.size(); ++i)
            {
                const transfer_details &td = m_transfers[i];
                m_pub_keys.emplace(td.get_public_key(), i);
            }
            return;
        }
        a & m_pub_keys;
        if(ver < 16)
            return;
        a & m_address_book;
        if(ver < 17)
            return;
        if (ver < 22)
        {
            // we're loading an old version, where m_unconfirmed_payments payload was payment_details
            std::unordered_multimap<crypto::hash, payment_details> m;
            a & m;
            m_unconfirmed_payments.clear();
            for (const auto &i: m)
                m_unconfirmed_payments.insert(std::make_pair(i.first, pool_payment_details{i.second, false}));
        }
        if(ver < 18)
            return;
        a & m_scanned_pool_txs[0];
        a & m_scanned_pool_txs[1];
        if (ver < 20)
            return;
        a & m_subaddresses;
        std::unordered_map<cryptonote::subaddress_index, crypto::public_key> dummy_subaddresses_inv;
        a & dummy_subaddresses_inv;
        a & m_subaddress_labels;
        a & m_additional_tx_keys;
        if(ver < 21)
            return;
        a & m_attributes;
        if(ver < 22)
            return;
        a & m_unconfirmed_payments;
        if(ver < 23)
            return;
        a & (std::pair<std::map<std::string, std::string>, std::vector<std::string>>&)m_account_tags;
        if(ver < 24)
            return;
        a & m_ring_history_saved;
        if(ver < 25)
            return;
        a & m_last_block_reward;
        if(ver < 26)
            return;
        a & m_tx_device;
        if(ver < 27)
            return;
        a & m_device_last_key_image_sync;
        if(ver < 28)
            return;
        a & m_cold_key_images;
        if(ver < 29)
            return;
        crypto::secret_key dummy_rpc_client_secret_key; // Compatibility for old RPC payment system
        a & dummy_rpc_client_secret_key;
        if(ver < 30)
        {
            m_has_ever_refreshed_from_node = false;
            return;
        }
        a & m_has_ever_refreshed_from_node;
    }

    BEGIN_SERIALIZE_OBJECT()
    MAGIC_FIELD("monero wallet cache")
    VERSION_FIELD(1)
    FIELD(m_blockchain)
    FIELD(m_transfers)
    FIELD(m_account_public_address)
    FIELD(m_key_images)
    FIELD(m_unconfirmed_txs)
    FIELD(m_payments)
    FIELD(m_tx_keys)
    FIELD(m_confirmed_txs)
    FIELD(m_tx_notes)
    FIELD(m_unconfirmed_payments)
    FIELD(m_pub_keys)
    FIELD(m_address_book)
    FIELD(m_scanned_pool_txs[0])
    FIELD(m_scanned_pool_txs[1])
    FIELD(m_subaddresses)
    FIELD(m_subaddress_labels)
    FIELD(m_additional_tx_keys)
    FIELD(m_attributes)
    FIELD(m_account_tags)
    FIELD(m_ring_history_saved)
    FIELD(m_last_block_reward)
    FIELD(m_tx_device)
    FIELD(m_device_last_key_image_sync)
    FIELD(m_cold_key_images)
    crypto::secret_key dummy_rpc_client_secret_key; // Compatibility for old RPC payment system
    FIELD_N("m_rpc_client_secret_key", dummy_rpc_client_secret_key)
    if (version < 1)
    {
        m_has_ever_refreshed_from_node = false;
        return true;
    }
    FIELD(m_has_ever_refreshed_from_node)
    END_SERIALIZE()


    WalletSettings(NetworkType nettype, uint64_t kdf_rounds, std::unique_ptr<epee::net_utils::http::http_client_factory> http_client_factory = std::unique_ptr<epee::net_utils::http::http_client_factory>(new net::http::client_factory()));
    /**
    * brief: clear -
    */
    void clear();
    /**
    * brief: get_account_tags -
    * return: m_account_tags -
    */
    const std::pair<std::map<std::string, std::string>, std::vector<std::string>>& get_account_tags(size_t num_subaddress_accounts);
    /**
    * brief: init_type -
    * param: device_type -
    * param: account -
    */
    void init_type(hw::device::device_type device_type, cryptonote::account_base &account);
    /**
    * brief: prepare_file_names -
    * param: file_path -
    */
    void prepare_file_names(const std::string &file_path);
    /**
    * brief: setup_new_blockchain -
    */
    void setup_new_blockchain();
    /**
    * brief: get_multisig_wallet_state -
    */
    mms::multisig_wallet_state get_multisig_wallet_state(cryptonote::account_base &account) const;
    bool has_multisig_partial_key_images() const;
    bool multisig(cryptonote::account_base &account, bool *ready = NULL, uint32_t *threshold = NULL, uint32_t *total = NULL) const;

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
    bool m_has_ever_refreshed_from_node;
    bool m_ignore_fractional_outputs;
    bool m_key_reuse_mitigation2;
    bool m_load_deprecated_formats;
    bool m_merge_destinations;
    bool m_multisig;
    bool m_original_keys_available;
    bool m_print_ring_members;
    bool m_ring_history_saved;
    bool m_show_wallet_name_when_locked;
    bool m_segregate_pre_fork_outputs;
    bool m_store_tx_info; /* request txkey to be returned in RPC, and store in the wallet cache file */
    bool m_track_uses;
    bool m_unattended;
    bool m_watch_only;

    crypto::secret_key m_original_view_secret_key;
    cryptonote::account_public_address m_account_public_address;
    cryptonote::account_public_address m_original_address;

    ExportFormat m_export_format;

    hashchain m_blockchain;

    hw::device::device_type m_key_device_type;

    mms::message_store m_message_store;

    NetworkType m_nettype;

    payment_container m_payments;

    RefreshType m_refresh_type;

    std::pair<std::map<std::string, std::string>, std::vector<std::string>> m_account_tags;

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

    std::uint64_t m_device_last_key_image_sync;
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

    std::unordered_set<crypto::hash> m_scanned_pool_txs[2];
    std::unordered_map<crypto::hash, confirmed_transfer_details> m_confirmed_txs;
    std::unordered_map<crypto::hash, crypto::secret_key> m_tx_keys;
    std::unordered_map<crypto::hash, std::string> m_tx_device;
    std::unordered_map<crypto::hash, std::string> m_tx_notes;
    std::unordered_map<crypto::hash, std::vector<crypto::secret_key>> m_additional_tx_keys;
    std::unordered_map<crypto::hash, unconfirmed_transfer_details> m_unconfirmed_txs;
    std::unordered_map<crypto::key_image, size_t> m_key_images;
    std::unordered_map<crypto::public_key, cryptonote::subaddress_index> m_subaddresses;
    std::unordered_map<crypto::public_key, crypto::key_image> m_cold_key_images;
    std::unordered_map<crypto::public_key, size_t> m_pub_keys;
    std::unordered_map<std::string, std::string> m_attributes;

    std::unordered_multimap<crypto::hash, pool_payment_details> m_unconfirmed_payments;

    std::vector<address_book_row> m_address_book;
    std::vector<crypto::public_key> m_multisig_derivations;
    std::vector<crypto::public_key> m_multisig_signers;
    std::vector<std::vector<std::string>> m_subaddress_labels;

    transfer_container m_transfers;
};


} // namespace
