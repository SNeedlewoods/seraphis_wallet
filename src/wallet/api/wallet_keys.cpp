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
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "device/device.hpp"
#include "wallet/wallet_errors.h"

//third party headers
#include "byte_slice.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "storages/portable_storage_template_helper.h"

//standard headers


namespace Monero
{
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static bool generate_chacha_key_from_secret_keys(crypto::chacha_key &key,
                                                 cryptonote::account_base &account,
                                                 std::uint64_t kdf_rounds)
{
    hw::device &hwdev = account.get_device();
    return hwdev.generate_chacha_key(account.get_keys(), key, kdf_rounds);
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
WalletKeys::WalletKeys()
{
}
//-------------------------------------------------------------------------------------------------------------------
void WalletKeys::create_keys_file(const std::string &wallet_,
                                  bool watch_only,
                                  const epee::wipeable_string &password,
                                  bool create_address_file,
                                  cryptonote::account_base &account,
                                  const std::unique_ptr<WalletSettings> &wallet_settings)
{
    if (!wallet_.empty())
    {
        bool r = store_keys(wallet_settings->m_keys_file, password, watch_only, account, wallet_settings);
        THROW_WALLET_EXCEPTION_IF(!r, tools::error::file_save_error, wallet_settings->m_keys_file);

        if (create_address_file)
        {
            // TODO NEXT 2
//            r = save_to_file(wallet_settings->m_wallet_file + ".address.txt", account.get_public_address_str(wallet_settings->m_nettype), true);
//            if(!r) MERROR("String with address text not saved");
        }
    }
}
//-------------------------------------------------------------------------------------------------------------------
boost::optional<WalletKeys::keys_file_data> WalletKeys::get_keys_file_data(const epee::wipeable_string &password,
                                                                           bool watch_only,
                                                                           cryptonote::account_base &account,
                                                                           const std::unique_ptr<WalletSettings> &wallet_settings)
{
    epee::byte_slice account_data;
    std::string multisig_signers;
    std::string multisig_derivations;

    crypto::chacha_key key;
    crypto::generate_chacha_key(password.data(), password.size(), key, wallet_settings->m_kdf_rounds);

    // We use m_cache_key as a deterministic test to see if given key corresponds to original password
    const crypto::chacha_key cache_key = Utils::derive_cache_key(key);
    THROW_WALLET_EXCEPTION_IF(cache_key != m_cache_key, tools::error::invalid_password);

    if (wallet_settings->m_ask_password == AskPasswordToDecrypt &&
       !wallet_settings->m_unattended &&
       !wallet_settings->m_watch_only)
    {
        account.encrypt_viewkey(key);
        // TODO : does it makes sense to decrypt here and encrypt later, or do we need decrypted keys to be able to forget_spend_key()?
        account.decrypt_keys(key);
    }

    if (watch_only)
        account.forget_spend_key();

    account.encrypt_keys(key);

    bool r = epee::serialization::store_t_to_binary(account, account_data);
    CHECK_AND_ASSERT_MES(r, boost::none, "failed to serialize wallet keys");
    boost::optional<WalletKeys::keys_file_data> keys_file_data = (WalletKeys::keys_file_data) {};

    // Create a JSON object with "key_data" and "seed_language" as keys.
    rapidjson::Document json;
    json.SetObject();
    rapidjson::Value value(rapidjson::kStringType);
    value.SetString(reinterpret_cast<const char*>(account_data.data()), account_data.size());
    json.AddMember("key_data", value, json.GetAllocator());
    if (!wallet_settings->m_seed_language.empty())
    {
        value.SetString(wallet_settings->m_seed_language.c_str(), wallet_settings->m_seed_language.length());
        json.AddMember("seed_language", value, json.GetAllocator());
    }

    rapidjson::Value value2(rapidjson::kNumberType);

    value2.SetInt(wallet_settings->m_key_device_type);
    json.AddMember("key_on_device", value2, json.GetAllocator());

    value2.SetInt((watch_only || wallet_settings->m_watch_only) ? 1 : 0);
    json.AddMember("watch_only", value2, json.GetAllocator());

    // TODO : multisig should get its own place
//    value2.SetInt(m_multisig ? 1 :0);
//    json.AddMember("multisig", value2, json.GetAllocator());
//
//    value2.SetUint(m_multisig_threshold);
//    json.AddMember("multisig_threshold", value2, json.GetAllocator());
//
//    if (m_multisig)
//    {
//        bool r = ::serialization::dump_binary(m_multisig_signers, multisig_signers);
//        CHECK_AND_ASSERT_MES(r, boost::none, "failed to serialize wallet multisig signers");
//        value.SetString(multisig_signers.c_str(), multisig_signers.length());
//        json.AddMember("multisig_signers", value, json.GetAllocator());
//
//        r = ::serialization::dump_binary(m_multisig_derivations, multisig_derivations);
//        CHECK_AND_ASSERT_MES(r, boost::none, "failed to serialize wallet multisig derivations");
//        value.SetString(multisig_derivations.c_str(), multisig_derivations.length());
//        json.AddMember("multisig_derivations", value, json.GetAllocator());
//
//        value2.SetUint(m_multisig_rounds_passed);
//        json.AddMember("multisig_rounds_passed", value2, json.GetAllocator());
//    }

    value2.SetInt(wallet_settings->m_always_confirm_transfers ? 1 : 0);
    json.AddMember("always_confirm_transfers", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_print_ring_members ? 1 : 0);
    json.AddMember("print_ring_members", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_store_tx_info ? 1 : 0);
    json.AddMember("store_tx_info", value2, json.GetAllocator());

    // TODO : deprecated? maybe needs to be handled like confirm_non_default_ring_size below for backwards compability
//    value2.SetUint(m_default_mixin);
//    json.AddMember("default_mixin", value2, json.GetAllocator());

    value2.SetUint(wallet_settings->m_default_priority);
    json.AddMember("default_priority", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_auto_refresh ? 1 : 0);
    json.AddMember("auto_refresh", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_refresh_type);
    json.AddMember("refresh_type", value2, json.GetAllocator());

    value2.SetUint64(wallet_settings->m_refresh_from_block_height);
    json.AddMember("refresh_height", value2, json.GetAllocator());

    value2.SetUint64(wallet_settings->m_skip_to_height);
    json.AddMember("skip_to_height", value2, json.GetAllocator());

    value2.SetInt(1); // exists for deserialization backwards compatibility
    json.AddMember("confirm_non_default_ring_size", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_ask_password);
    json.AddMember("ask_password", value2, json.GetAllocator());

    value2.SetUint64(wallet_settings->m_max_reorg_depth);
    json.AddMember("max_reorg_depth", value2, json.GetAllocator());

    value2.SetUint(wallet_settings->m_min_output_count);
    json.AddMember("min_output_count", value2, json.GetAllocator());

    value2.SetUint64(wallet_settings->m_min_output_value);
    json.AddMember("min_output_value", value2, json.GetAllocator());

    value2.SetInt(cryptonote::get_default_decimal_point());
    json.AddMember("default_decimal_point", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_merge_destinations ? 1 : 0);
    json.AddMember("merge_destinations", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_confirm_backlog ? 1 : 0);
    json.AddMember("confirm_backlog", value2, json.GetAllocator());

    value2.SetUint(wallet_settings->m_confirm_backlog_threshold);
    json.AddMember("confirm_backlog_threshold", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_confirm_export_overwrite ? 1 : 0);
    json.AddMember("confirm_export_overwrite", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_auto_low_priority ? 1 : 0);
    json.AddMember("auto_low_priority", value2, json.GetAllocator());

    value2.SetUint(wallet_settings->m_nettype);
    json.AddMember("nettype", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_segregate_pre_fork_outputs ? 1 : 0);
    json.AddMember("segregate_pre_fork_outputs", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_key_reuse_mitigation2 ? 1 : 0);
    json.AddMember("key_reuse_mitigation2", value2, json.GetAllocator());

    value2.SetUint(wallet_settings->m_segregation_height);
    json.AddMember("segregation_height", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_ignore_fractional_outputs ? 1 : 0);
    json.AddMember("ignore_fractional_outputs", value2, json.GetAllocator());

    value2.SetUint64(wallet_settings->m_ignore_outputs_above);
    json.AddMember("ignore_outputs_above", value2, json.GetAllocator());

    value2.SetUint64(wallet_settings->m_ignore_outputs_below);
    json.AddMember("ignore_outputs_below", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_track_uses ? 1 : 0);
    json.AddMember("track_uses", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_show_wallet_name_when_locked ? 1 : 0);
    json.AddMember("show_wallet_name_when_locked", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_inactivity_lock_timeout);
    json.AddMember("inactivity_lock_timeout", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_setup_background_mining);
    json.AddMember("setup_background_mining", value2, json.GetAllocator());

    value2.SetUint(wallet_settings->m_subaddress_lookahead_major);
    json.AddMember("subaddress_lookahead_major", value2, json.GetAllocator());

    value2.SetUint(wallet_settings->m_subaddress_lookahead_minor);
    json.AddMember("subaddress_lookahead_minor", value2, json.GetAllocator());

    // TODO : afaik this is multisig related and can get removed
//  value2.SetInt(m_original_keys_available ? 1 : 0);
//  json.AddMember("original_keys_available", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_export_format);
    json.AddMember("export_format", value2, json.GetAllocator());

    value2.SetInt(wallet_settings->m_load_deprecated_formats);
    json.AddMember("load_deprecated_formats", value2, json.GetAllocator());

    value2.SetUint(1);
    json.AddMember("encrypted_secret_keys", value2, json.GetAllocator());

    value.SetString(wallet_settings->m_device_name.c_str(), wallet_settings->m_device_name.size());
    json.AddMember("device_name", value, json.GetAllocator());

    value.SetString(wallet_settings->m_device_derivation_path.c_str(), wallet_settings->m_device_derivation_path.size());
    json.AddMember("device_derivation_path", value, json.GetAllocator());

    // TODO : afaik this is also multisig related
//  std::string original_address;
//  std::string original_view_secret_key;
//  if (m_original_keys_available)
//  {
//    original_address = get_account_address_as_str(m_nettype, false, m_original_address);
//    value.SetString(original_address.c_str(), original_address.length());
//    json.AddMember("original_address", value, json.GetAllocator());
//    original_view_secret_key = epee::string_tools::pod_to_hex(m_original_view_secret_key);
//    value.SetString(original_view_secret_key.c_str(), original_view_secret_key.length());
//    json.AddMember("original_view_secret_key", value, json.GetAllocator());
//  }

    // TODO : not sure about the following three items (iirc rpc pay is deprecated!?)
    // This value is serialized for compatibility with wallets which support the pay-to-use RPC system
    value2.SetInt(0);
    json.AddMember("persistent_rpc_client_id", value2, json.GetAllocator());

    // This value is serialized for compatibility with wallets which support the pay-to-use RPC system
    value2.SetFloat(0.0f);
    json.AddMember("auto_mine_for_rpc_payment", value2, json.GetAllocator());

    // This value is serialized for compatibility with wallets which support the pay-to-use RPC system
    value2.SetUint64(0);
    json.AddMember("credits_target", value2, json.GetAllocator());

//  value2.SetInt(m_enable_multisig ? 1 : 0);
//  json.AddMember("enable_multisig", value2, json.GetAllocator());

    // Serialize the JSON object
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json.Accept(writer);

    // Encrypt the entire JSON object.
    std::string cipher;
    cipher.resize(buffer.GetSize());
    keys_file_data.get().iv = crypto::rand<crypto::chacha_iv>();
    crypto::chacha20(buffer.GetString(), buffer.GetSize(), key, keys_file_data.get().iv, &cipher[0]);
    keys_file_data.get().account_data = cipher;
    return keys_file_data;
}
//-------------------------------------------------------------------------------------------------------------------
crypto::chacha_key WalletKeys::get_ringdb_key(cryptonote::account_base &account, const std::uint64_t kdf_rounds)
{
  if (!m_ringdb_key)
  {
    MINFO("caching ringdb key");
    crypto::chacha_key key;
    generate_chacha_key_from_secret_keys(key, account, kdf_rounds);
    m_ringdb_key = key;
  }
  return *m_ringdb_key;
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

    get_ringdb_key(account, wallet_settings->m_kdf_rounds);
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletKeys::store_keys(const std::string &keys_file_name,
                            const epee::wipeable_string &password,
                            bool watch_only,
                            cryptonote::account_base &account,
                            const std::unique_ptr<WalletSettings> &wallet_settings)
{
  boost::optional<WalletKeys::keys_file_data> keys_file_data = get_keys_file_data(password, watch_only, account, wallet_settings);
  // TODO : maybe add a check like this, because afaiui get_keys_file_data() won't return boost::none
//  CHECK_AND_ASSERT_MES(keys_file_data.get() != (WalletKeys::keys_file_data) {}, false, "failed to generate wallet keys data");
  CHECK_AND_ASSERT_MES(keys_file_data != boost::none, false, "failed to generate wallet keys data");

    // TODO NEXT 1
//  std::string tmp_file_name = keys_file_name + ".new";
//  std::string buf;
//  bool r = ::serialization::dump_binary(keys_file_data.get(), buf);
//  r = r && save_to_file(tmp_file_name, buf);
//  CHECK_AND_ASSERT_MES(r, false, "failed to generate wallet keys file " << tmp_file_name);
//
//  unlock_keys_file();
//  std::error_code e = tools::replace_file(tmp_file_name, keys_file_name);
//  lock_keys_file();
//
//  if (e) {
//    boost::filesystem::remove(tmp_file_name);
//    LOG_ERROR("failed to update wallet keys file " << keys_file_name);
//    return false;
//  }
//
  return true;
}
//-------------------------------------------------------------------------------------------------------------------

} // namespace
