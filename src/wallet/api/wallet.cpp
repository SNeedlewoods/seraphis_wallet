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


#include "wallet.h"
#include "pending_transaction.h"
#include "unsigned_transaction.h"
#include "transaction_history.h"
#include "address_book.h"
#include "subaddress.h"
#include "subaddress_account.h"
#include "common_defines.h"
#include "common/util.h"
#include "multisig/multisig_account.h"

#include "mnemonics/electrum-words.h"
#include "mnemonics/english.h"
#include <boost/format.hpp>
#include <sstream>
#include <unordered_map>

#ifdef WIN32
#include <boost/locale.hpp>
#endif

#include <boost/filesystem.hpp>

using namespace std;
using namespace cryptonote;

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "WalletAPI"

#define LOCK_REFRESH() \
    bool refresh_enabled = m_refreshEnabled; \
    m_refreshEnabled = false; \
    stop(); \
    m_refreshCV.notify_one(); \
    boost::mutex::scoped_lock lock(m_refreshMutex); \
    boost::mutex::scoped_lock lock2(m_refreshMutex2); \
    epee::misc_utils::auto_scope_leave_caller scope_exit_handler = epee::misc_utils::create_scope_leave_handler([&](){ \
        /* m_refreshMutex's still locked here */ \
        if (refresh_enabled) \
            startRefresh(); \
    })

#define PRE_VALIDATE_BACKGROUND_SYNC() \
  do \
  { \
    clearStatus(); \
    if (m_wallet->key_on_device()) \
    { \
        setStatusError(tr("HW wallet cannot use background sync")); \
        return false; \
    } \
    if (watchOnly()) \
    { \
        setStatusError(tr("View only wallet cannot use background sync")); \
        return false; \
    } \
    if (m_wallet->get_multisig_status().multisig_is_active) \
    { \
        setStatusError(tr("Multisig wallet cannot use background sync")); \
        return false; \
    } \
  } while (0)

namespace Monero {

namespace {
    // copy-pasted from simplewallet
    static const int    DEFAULT_REFRESH_INTERVAL_MILLIS = 1000 * 10;
    // limit maximum refresh interval as one minute
    static const int    MAX_REFRESH_INTERVAL_MILLIS = 1000 * 60 * 1;
    // Default refresh interval when connected to remote node
    static const int    DEFAULT_REMOTE_NODE_REFRESH_INTERVAL_MILLIS = 1000 * 10;
    // Connection timeout 20 sec
    static const int    DEFAULT_CONNECTION_TIMEOUT_MILLIS = 1000 * 20;

    std::string get_default_ringdb_path(cryptonote::network_type nettype)
    {
      boost::filesystem::path dir = tools::get_default_data_dir();
      // remove .bitmonero, replace with .shared-ringdb
      dir = dir.remove_filename();
      dir /= ".shared-ringdb";
      if (nettype == cryptonote::TESTNET)
        dir /= "testnet";
      else if (nettype == cryptonote::STAGENET)
        dir /= "stagenet";
      return dir.string();
    }

    void checkMultisigWalletReady(const tools::wallet2* wallet) {
        if (!wallet) {
            throw runtime_error("Wallet is not initialized yet");
        }

        const multisig::multisig_account_status ms_status{wallet->get_multisig_status()};

        if (!ms_status.multisig_is_active) {
            throw runtime_error("Wallet is not multisig");
        }

        if (!ms_status.is_ready) {
            throw runtime_error("Multisig wallet is not finalized yet");
        }
    }
    void checkMultisigWalletReady(const std::unique_ptr<tools::wallet2> &wallet) {
        return checkMultisigWalletReady(wallet.get());
    }

    void checkMultisigWalletNotReady(const tools::wallet2* wallet) {
        if (!wallet) {
            throw runtime_error("Wallet is not initialized yet");
        }

        const multisig::multisig_account_status ms_status{wallet->get_multisig_status()};

        if (!ms_status.multisig_is_active) {
            throw runtime_error("Wallet is not multisig");
        }

        if (ms_status.is_ready) {
            throw runtime_error("Multisig wallet is already finalized");
        }
    }
    void checkMultisigWalletNotReady(const std::unique_ptr<tools::wallet2> &wallet) {
        return checkMultisigWalletNotReady(wallet.get());
    }
}

struct Wallet2CallbackImpl : public tools::i_wallet2_callback
{

    Wallet2CallbackImpl(WalletImpl * wallet)
     : m_listener(nullptr)
     , m_wallet(wallet)
    {

    }

    ~Wallet2CallbackImpl()
    {

    }

    void setListener(WalletListener * listener)
    {
        m_listener = listener;
    }

    WalletListener * getListener() const
    {
        return m_listener;
    }

    virtual void on_new_block(uint64_t height, const cryptonote::block& block)
    {
        // Don't flood the GUI with signals. On fast refresh - send signal every 1000th block
        // get_refresh_from_block_height() returns the blockheight from when the wallet was 
        // created or the restore height specified when wallet was recovered
        if(height >= m_wallet->m_wallet->get_refresh_from_block_height() || height % 1000 == 0) {
            // LOG_PRINT_L3(__FUNCTION__ << ": new block. height: " << height);
            if (m_listener) {
                m_listener->newBlock(height);
            }
        }
    }

    virtual void on_money_received(uint64_t height, const crypto::hash &txid, const cryptonote::transaction& tx, uint64_t amount, uint64_t burnt, const cryptonote::subaddress_index& subaddr_index, bool is_change, uint64_t unlock_time)
    {

        std::string tx_hash =  epee::string_tools::pod_to_hex(txid);

        LOG_PRINT_L3(__FUNCTION__ << ": money received. height:  " << height
                     << ", tx: " << tx_hash
                     << ", amount: " << print_money(amount - burnt)
                     << ", burnt: " << print_money(burnt)
                     << ", raw_output_value: " << print_money(amount)
                     << ", idx: " << subaddr_index);
        // do not signal on received tx if wallet is not syncronized completely
        if (m_listener && m_wallet->synchronized()) {
            m_listener->moneyReceived(tx_hash, amount - burnt);
            m_listener->updated();
        }
    }

    virtual void on_unconfirmed_money_received(uint64_t height, const crypto::hash &txid, const cryptonote::transaction& tx, uint64_t amount, const cryptonote::subaddress_index& subaddr_index)
    {

        std::string tx_hash =  epee::string_tools::pod_to_hex(txid);

        LOG_PRINT_L3(__FUNCTION__ << ": unconfirmed money received. height:  " << height
                     << ", tx: " << tx_hash
                     << ", amount: " << print_money(amount)
                     << ", idx: " << subaddr_index);
        // do not signal on received tx if wallet is not syncronized completely
        if (m_listener && m_wallet->synchronized()) {
            m_listener->unconfirmedMoneyReceived(tx_hash, amount);
            m_listener->updated();
        }
    }

    virtual void on_money_spent(uint64_t height, const crypto::hash &txid, const cryptonote::transaction& in_tx,
                                uint64_t amount, const cryptonote::transaction& spend_tx, const cryptonote::subaddress_index& subaddr_index)
    {
        // TODO;
        std::string tx_hash = epee::string_tools::pod_to_hex(txid);
        LOG_PRINT_L3(__FUNCTION__ << ": money spent. height:  " << height
                     << ", tx: " << tx_hash
                     << ", amount: " << print_money(amount)
                     << ", idx: " << subaddr_index);
        // do not signal on sent tx if wallet is not syncronized completely
        if (m_listener && m_wallet->synchronized()) {
            m_listener->moneySpent(tx_hash, amount);
            m_listener->updated();
        }
    }

    virtual void on_skip_transaction(uint64_t height, const crypto::hash &txid, const cryptonote::transaction& tx)
    {
        // TODO;
    }

    virtual void on_device_button_request(uint64_t code)
    {
      if (m_listener) {
        m_listener->onDeviceButtonRequest(code);
      }
    }

    virtual void on_device_button_pressed()
    {
      if (m_listener) {
        m_listener->onDeviceButtonPressed();
      }
    }

    virtual boost::optional<epee::wipeable_string> on_device_pin_request()
    {
      if (m_listener) {
        auto pin = m_listener->onDevicePinRequest();
        if (pin){
          return boost::make_optional(epee::wipeable_string((*pin).data(), (*pin).size()));
        }
      }
      return boost::none;
    }

    virtual boost::optional<epee::wipeable_string> on_device_passphrase_request(bool & on_device)
    {
      if (m_listener) {
        auto passphrase = m_listener->onDevicePassphraseRequest(on_device);
        if (passphrase) {
          return boost::make_optional(epee::wipeable_string((*passphrase).data(), (*passphrase).size()));
        }
      } else {
        on_device = true;
      }
      return boost::none;
    }

    virtual void on_device_progress(const hw::device_progress & event)
    {
      if (m_listener) {
        m_listener->onDeviceProgress(DeviceProgress(event.progress(), event.indeterminate()));
      }
    }

    virtual void on_reorg(uint64_t height, uint64_t blocks_detached, size_t transfers_detached) { /* TODO */ }
    virtual boost::optional<epee::wipeable_string> on_get_password(const char *reason) { return boost::none; }
    virtual void on_pool_tx_removed(const crypto::hash &txid) { /* TODO */ }

    WalletListener * m_listener;
    WalletImpl     * m_wallet;
};

Wallet::~Wallet() {}

WalletListener::~WalletListener() {}


string Wallet::displayAmount(uint64_t amount)
{
    return cryptonote::print_money(amount);
}

uint64_t Wallet::amountFromString(const string &amount)
{
    uint64_t result = 0;
    cryptonote::parse_amount(result, amount);
    return result;
}

uint64_t Wallet::amountFromDouble(double amount)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(CRYPTONOTE_DISPLAY_DECIMAL_POINT) << amount;
    return amountFromString(ss.str());
}

std::string Wallet::genPaymentId()
{
    crypto::hash8 payment_id = crypto::rand<crypto::hash8>();
    return epee::string_tools::pod_to_hex(payment_id);

}

bool Wallet::paymentIdValid(const string &paiment_id)
{
    crypto::hash8 pid8;
    if (tools::wallet2::parse_short_payment_id(paiment_id, pid8))
        return true;
    crypto::hash pid;
    if (tools::wallet2::parse_long_payment_id(paiment_id, pid))
        return true;
    return false;
}

bool Wallet::addressValid(const std::string &str, NetworkType nettype)
{
  cryptonote::address_parse_info info;
  return get_account_address_from_str(info, static_cast<cryptonote::network_type>(nettype), str);
}

bool Wallet::keyValid(const std::string &secret_key_string, const std::string &address_string, bool isViewKey, NetworkType nettype, std::string &error)
{
  cryptonote::address_parse_info info;
  if(!get_account_address_from_str(info, static_cast<cryptonote::network_type>(nettype), address_string)) {
      error = tr("Failed to parse address");
      return false;
  }
  
  cryptonote::blobdata key_data;
  if(!epee::string_tools::parse_hexstr_to_binbuff(secret_key_string, key_data) || key_data.size() != sizeof(crypto::secret_key))
  {
      error = tr("Failed to parse key");
      return false;
  }
  crypto::secret_key key = *reinterpret_cast<const crypto::secret_key*>(key_data.data());

  // check the key match the given address
  crypto::public_key pkey;
  if (!crypto::secret_key_to_public_key(key, pkey)) {
      error = tr("failed to verify key");
      return false;
  }
  bool matchAddress = false;
  if(isViewKey)
      matchAddress = info.address.m_view_public_key == pkey;
  else
      matchAddress = info.address.m_spend_public_key == pkey;

  if(!matchAddress) {
      error = tr("key does not match address");
      return false;
  }
  
  return true;
}

std::string Wallet::paymentIdFromAddress(const std::string &str, NetworkType nettype)
{
  cryptonote::address_parse_info info;
  if (!get_account_address_from_str(info, static_cast<cryptonote::network_type>(nettype), str))
    return "";
  if (!info.has_payment_id)
    return "";
  return epee::string_tools::pod_to_hex(info.payment_id);
}

uint64_t Wallet::maximumAllowedAmount()
{
    return std::numeric_limits<uint64_t>::max();
}

void Wallet::init(const char *argv0, const char *default_log_base_name, const std::string &log_path, bool console) {
#ifdef WIN32
    // Activate UTF-8 support for Boost filesystem classes on Windows
    std::locale::global(boost::locale::generator().generate(""));
    boost::filesystem::path::imbue(std::locale());
#endif
    epee::string_tools::set_module_name_and_folder(argv0);
    mlog_configure(log_path.empty() ? mlog_get_default_log_path(default_log_base_name) : log_path.c_str(), console);
}

void Wallet::debug(const std::string &category, const std::string &str) {
    MCDEBUG(category.empty() ? MONERO_DEFAULT_LOG_CATEGORY : category.c_str(), str);
}

void Wallet::info(const std::string &category, const std::string &str) {
    MCINFO(category.empty() ? MONERO_DEFAULT_LOG_CATEGORY : category.c_str(), str);
}

void Wallet::warning(const std::string &category, const std::string &str) {
    MCWARNING(category.empty() ? MONERO_DEFAULT_LOG_CATEGORY : category.c_str(), str);
}

void Wallet::error(const std::string &category, const std::string &str) {
    MCERROR(category.empty() ? MONERO_DEFAULT_LOG_CATEGORY : category.c_str(), str);
}

///////////////////////// WalletImpl implementation ////////////////////////
WalletImpl::WalletImpl(NetworkType nettype, uint64_t kdf_rounds)
    :m_wallet(nullptr)
    , m_status(Wallet::Status_Ok)
    , m_wallet2Callback(nullptr)
    , m_recoveringFromSeed(false)
    , m_recoveringFromDevice(false)
    , m_synchronized(false)
    , m_rebuildWalletCache(false)
    , m_is_connected(false)
    , m_refreshShouldRescan(false)
{
    m_wallet.reset(new tools::wallet2(static_cast<cryptonote::network_type>(nettype), kdf_rounds, true));
    m_history.reset(new TransactionHistoryImpl(this));
    m_wallet2Callback.reset(new Wallet2CallbackImpl(this));
    m_wallet->callback(m_wallet2Callback.get());
    m_refreshThreadDone = false;
    m_refreshEnabled = false;
    m_addressBook.reset(new AddressBookImpl(this));
    m_subaddress.reset(new SubaddressImpl(this));
    m_subaddressAccount.reset(new SubaddressAccountImpl(this));


    m_refreshIntervalMillis = DEFAULT_REFRESH_INTERVAL_MILLIS;

    m_refreshThread = boost::thread([this] () {
        this->refreshThreadFunc();
    });

}

WalletImpl::~WalletImpl()
{

    LOG_PRINT_L1(__FUNCTION__);
    m_wallet->callback(NULL);
    // Pause refresh thread - prevents refresh from starting again
    WalletImpl::pauseRefresh(); // Call the method directly (not polymorphically) to protect against UB in destructor.
    // Close wallet - stores cache and stops ongoing refresh operation 
    close(false); // do not store wallet as part of the closing activities
    // Stop refresh thread
    stopRefresh();

    if (m_wallet2Callback->getListener()) {
      m_wallet2Callback->getListener()->onSetWallet(nullptr);
    }

    LOG_PRINT_L1(__FUNCTION__ << " finished");
}

bool WalletImpl::create(const std::string &path, const std::string &password, const std::string &language)
{

    clearStatus();
    m_recoveringFromSeed = false;
    m_recoveringFromDevice = false;
    bool keys_file_exists;
    bool wallet_file_exists;
    // QUESTION : I think we can change `WalletManagerImpl::walletExists()` which returns true if `key_file_exists` is true, to take the same outparams as `tools::wallet2::wallet_exists()` (but make them optional) and make the method static. Or do you think it's easier/cleaner to add `WalletImpl::walletExists()` with the described behaviour and leave the one in `WalletManagerImpl` as is?
    tools::wallet2::wallet_exists(path, keys_file_exists, wallet_file_exists);
    LOG_PRINT_L3("wallet_path: " << path << "");
    LOG_PRINT_L3("keys_file_exists: " << std::boolalpha << keys_file_exists << std::noboolalpha
                 << "  wallet_file_exists: " << std::boolalpha << wallet_file_exists << std::noboolalpha);


    // add logic to error out if new wallet requested but named wallet file exists
    if (keys_file_exists || wallet_file_exists) {
        std::string error = "attempting to generate or restore wallet, but specified file(s) exist.  Exiting to not risk overwriting.";
        LOG_ERROR(error);
        setStatusCritical(error);
        return false;
    }
    // TODO: validate language
    setSeedLanguage(language);
    crypto::secret_key recovery_val, secret_key;
    try {
        recovery_val = m_wallet->generate(path, password, secret_key, false, false);
        m_password = password;
        clearStatus();
    } catch (const std::exception &e) {
        LOG_ERROR("Error creating wallet: " << e.what());
        setStatusCritical(e.what());
        return false;
    }

    return true;
}

bool WalletImpl::createWatchOnly(const std::string &path, const std::string &password, const std::string &language) const
{
    clearStatus();
    std::unique_ptr<tools::wallet2> view_wallet(new tools::wallet2(m_wallet->nettype()));

    // Store same refresh height as original wallet
    view_wallet->set_refresh_from_block_height(m_wallet->get_refresh_from_block_height());

    bool keys_file_exists;
    bool wallet_file_exists;
    // QUESTION / TODO : same as above
    tools::wallet2::wallet_exists(path, keys_file_exists, wallet_file_exists);
    LOG_PRINT_L3("wallet_path: " << path << "");
    LOG_PRINT_L3("keys_file_exists: " << std::boolalpha << keys_file_exists << std::noboolalpha
                 << "  wallet_file_exists: " << std::boolalpha << wallet_file_exists << std::noboolalpha);

    // add logic to error out if new wallet requested but named wallet file exists
    if (keys_file_exists || wallet_file_exists) {
        std::string error = "attempting to generate view only wallet, but specified file(s) exist.  Exiting to not risk overwriting.";
        LOG_ERROR(error);
        setStatusError(error);
        return false;
    }
    // TODO : Should we create a `WalletImpl` object for `view_wallet` instead of just using the `wallet2`? Figure out if that's even possible without much hassle. Then we can use the proposed `setSeedLanguage()` which also validates the language.
    // TODO: validate language
    view_wallet->set_seed_language(language);

    const crypto::secret_key viewkey = m_wallet->get_account().get_keys().m_view_secret_key;
    const cryptonote::account_public_address address = m_wallet->get_account().get_keys().m_account_address;

    try {
        // Generate view only wallet
        view_wallet->generate(path, password, address, viewkey);

        // Export/Import outputs
        // TODO : Depends on question from above, if `view_wallet` becomes `WalletImpl`.
//        std::string outputs = exportOutputsToStr(true/*all*/);
//        view_wallet->importOutputsFromStr(outputs);
        auto outputs = m_wallet->export_outputs(true/*all*/);
        view_wallet->import_outputs(outputs);

        // Copy scanned blockchain
        auto bc = m_wallet->export_blockchain();
        view_wallet->import_blockchain(bc);

        // copy payments
        auto payments = m_wallet->export_payments();
        view_wallet->import_payments(payments);

        // copy confirmed outgoing payments
        std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>> out_payments;
        // TODO : Depends on question from above, if `view_wallet` becomes `WalletImpl` & `getPaymentsOut()` is not implemented yet
        m_wallet->get_payments_out(out_payments, 0);
        view_wallet->import_payments_out(out_payments);

        // Export/Import key images
        // We already know the spent status from the outputs we exported, thus no need to check them again
        // TODO : `exportKeyImages()` is only implemented to export to file, consider adding `exportKeyImagesToStr()` or something.
        auto key_images = m_wallet->export_key_images(true/*all*/);
        uint64_t spent = 0;
        uint64_t unspent = 0;
        view_wallet->import_key_images(key_images.second, key_images.first, spent, unspent, false);
        clearStatus();
    } catch (const std::exception &e) {
        LOG_ERROR("Error creating view only wallet: " << e.what());
        setStatusError(e.what());
        return false;
    }
    // Store wallet
    // TODO : Depends on question from above, if `view_wallet` becomes `WalletImpl`.
    view_wallet->store();
    return true;
}

bool WalletImpl::recoverFromKeys(const std::string &path,
                                const std::string &language,
                                const std::string &address_string,
                                const std::string &viewkey_string,
                                const std::string &spendkey_string)
{
    return recoverFromKeysWithPassword(path, "", language, address_string, viewkey_string, spendkey_string);
}

bool WalletImpl::recoverFromKeysWithPassword(const std::string &path,
                                 const std::string &password,
                                 const std::string &language,
                                 const std::string &address_string,
                                 const std::string &viewkey_string,
                                 const std::string &spendkey_string)
{
    cryptonote::address_parse_info info;
    if(!get_account_address_from_str(info, m_wallet->nettype(), address_string))
    {
        setStatusError(tr("failed to parse address"));
        return false;
    }

    // parse optional spend key
    crypto::secret_key spendkey;
    bool has_spendkey = false;
    if (!spendkey_string.empty()) {
        cryptonote::blobdata spendkey_data;
        if(!epee::string_tools::parse_hexstr_to_binbuff(spendkey_string, spendkey_data) || spendkey_data.size() != sizeof(crypto::secret_key))
        {
            setStatusError(tr("failed to parse secret spend key"));
            return false;
        }
        has_spendkey = true;
        spendkey = *reinterpret_cast<const crypto::secret_key*>(spendkey_data.data());
    }

    // parse view secret key
    bool has_viewkey = true;
    crypto::secret_key viewkey;
    if (viewkey_string.empty()) {
        if(has_spendkey) {
          has_viewkey = false;
        }
        else {
          setStatusError(tr("Neither view key nor spend key supplied, cancelled"));
          return false;
        }
    }
    if(has_viewkey) {
      cryptonote::blobdata viewkey_data;
      if(!epee::string_tools::parse_hexstr_to_binbuff(viewkey_string, viewkey_data) || viewkey_data.size() != sizeof(crypto::secret_key))
      {
          setStatusError(tr("failed to parse secret view key"));
          return false;
      }
      viewkey = *reinterpret_cast<const crypto::secret_key*>(viewkey_data.data());
    }
    // check the spend and view keys match the given address
    crypto::public_key pkey;
    if(has_spendkey) {
        if (!crypto::secret_key_to_public_key(spendkey, pkey)) {
            setStatusError(tr("failed to verify secret spend key"));
            return false;
        }
        if (info.address.m_spend_public_key != pkey) {
            setStatusError(tr("spend key does not match address"));
            return false;
        }
    }
    if(has_viewkey) {
       if (!crypto::secret_key_to_public_key(viewkey, pkey)) {
           setStatusError(tr("failed to verify secret view key"));
           return false;
       }
       if (info.address.m_view_public_key != pkey) {
           setStatusError(tr("view key does not match address"));
           return false;
       }
    }

    try
    {
        if (has_spendkey && has_viewkey) {
            m_wallet->generate(path, password, info.address, spendkey, viewkey);
            LOG_PRINT_L1("Generated new wallet from spend key and view key");
        }
        if(!has_spendkey && has_viewkey) {
            m_wallet->generate(path, password, info.address, viewkey);
            LOG_PRINT_L1("Generated new view only wallet from keys");
        }
        if(has_spendkey && !has_viewkey) {
           m_wallet->generate(path, password, spendkey, true, false);
           setSeedLanguage(language);
           LOG_PRINT_L1("Generated deterministic wallet from spend key with seed language: " + language);
        }
        
    }
    catch (const std::exception& e) {
        setStatusError(string(tr("failed to generate new wallet: ")) + e.what());
        return false;
    }
    return true;
}

bool WalletImpl::recoverFromDevice(const std::string &path, const std::string &password, const std::string &device_name)
{
    clearStatus();
    m_recoveringFromSeed = false;
    m_recoveringFromDevice = true;
    try
    {
        m_wallet->restore(path, password, device_name);
        LOG_PRINT_L1("Generated new wallet from device: " + device_name);
    }
    catch (const std::exception& e) {
        setStatusError(string(tr("failed to generate new wallet: ")) + e.what());
        return false;
    }
    return true;
}

Wallet::Device WalletImpl::getDeviceType() const
{
    return static_cast<Wallet::Device>(m_wallet->get_device_type());
}

bool WalletImpl::open(const std::string &path, const std::string &password)
{
    clearStatus();
    m_recoveringFromSeed = false;
    m_recoveringFromDevice = false;
    try {
        // TODO: handle "deprecated"
        // Check if wallet cache exists
        bool keys_file_exists;
        bool wallet_file_exists;
        // QUESTION / TODO : same as above, actually here we could just do:
//        if(!WalletManagerImpl::walletExists(path)){
        // even though we check if `wallet_file_exists`, the comment states that we're actually interested in the .keys file.
        tools::wallet2::wallet_exists(path, keys_file_exists, wallet_file_exists);
        if(!wallet_file_exists){
            // Rebuilding wallet cache, using refresh height from .keys file
            m_rebuildWalletCache = true;
        }
        m_wallet->set_ring_database(get_default_ringdb_path(m_wallet->nettype()));
        m_wallet->load(path, password);

        m_password = password;
    } catch (const std::exception &e) {
        LOG_ERROR("Error opening wallet: " << e.what());
        setStatusCritical(e.what());
    }
    return status() == Status_Ok;
}

bool WalletImpl::recover(const std::string &path, const std::string &seed)
{
    return recover(path, "", seed);
}

bool WalletImpl::recover(const std::string &path, const std::string &password, const std::string &seed, const std::string &seed_offset/* = {}*/)
{
    clearStatus();
    m_errorString.clear();
    if (seed.empty()) {
        LOG_ERROR("Electrum seed is empty");
        setStatusError(tr("Electrum seed is empty"));
        return false;
    }

    m_recoveringFromSeed = true;
    m_recoveringFromDevice = false;
    crypto::secret_key recovery_key;
    std::string old_language;
    if (!crypto::ElectrumWords::words_to_bytes(seed, recovery_key, old_language)) {
        setStatusError(tr("Electrum-style word list failed verification"));
        return false;
    }
    if (!seed_offset.empty())
    {
        recovery_key = cryptonote::decrypt_key(recovery_key, seed_offset);
    }

    if (old_language == crypto::ElectrumWords::old_language_name)
        old_language = Language::English().get_language_name();

    try {
        setSeedLanguage(old_language);
        m_wallet->generate(path, password, recovery_key, true, false);

    } catch (const std::exception &e) {
        setStatusCritical(e.what());
    }
    return status() == Status_Ok;
}

bool WalletImpl::close(bool store)
{

    bool result = false;
    LOG_PRINT_L1("closing wallet...");
    try {
        if (store) {
            // Do not store wallet with invalid status
            // Status Critical refers to errors on opening or creating wallets.
            if (status() != Status_Critical)
                store();
            else
                LOG_ERROR("Status_Critical - not saving wallet");
            LOG_PRINT_L1("wallet::store done");
        }
        LOG_PRINT_L1("Calling wallet::stop...");
        stop();
        LOG_PRINT_L1("wallet::stop done");
        m_wallet->deinit();
        result = true;
        clearStatus();
    } catch (const std::exception &e) {
        setStatusCritical(e.what());
        LOG_ERROR("Error closing wallet: " << e.what());
    }
    return result;
}

std::string WalletImpl::seed(const std::string& seed_offset) const
{
    if (checkBackgroundSync("cannot get seed"))
        return std::string();
    epee::wipeable_string seed;
    if (m_wallet)
        m_wallet->get_seed(seed, seed_offset);
    return std::string(seed.data(), seed.size()); // TODO
}

std::string WalletImpl::getSeedLanguage() const
{
    return m_wallet->get_seed_language();
}

void WalletImpl::setSeedLanguage(const std::string &arg)
{
    if (checkBackgroundSync("cannot set seed language"))
        return;

    // QUESTION : We have some ~8year old TODOs stating we should validate the seed language. Will this suffice or should I make another PR for that?
    //            IMO it'd make sense now to add `setStatusError()` to this so it doesn't just silently do nothing. If this can stay, remove old TODOs.
    // Update : Actually after rebasing we now already set the status in the `checkBackgroundSync()` call above.
    if (crypto::ElectrumWords::is_valid_language(arg))
        m_wallet->set_seed_language(arg);
}

int WalletImpl::status() const
{
    boost::lock_guard<boost::mutex> l(m_statusMutex);
    return m_status;
}

std::string WalletImpl::errorString() const
{
    boost::lock_guard<boost::mutex> l(m_statusMutex);
    return m_errorString;
}

void WalletImpl::statusWithErrorString(int& status, std::string& errorString) const {
    boost::lock_guard<boost::mutex> l(m_statusMutex);
    status = m_status;
    errorString = m_errorString;
}

bool WalletImpl::setPassword(const std::string &password)
{
    if (checkBackgroundSync("cannot change password"))
        return false;
    clearStatus();
    try {
        m_wallet->change_password(m_wallet->get_wallet_file(), m_password, password);
        m_password = password;
    } catch (const std::exception &e) {
        setStatusError(e.what());
    }
    return status() == Status_Ok;
}

const std::string& WalletImpl::getPassword() const
{
    return m_password;
}

bool WalletImpl::setDevicePin(const std::string &pin)
{
    clearStatus();
    try {
        m_wallet->get_account().get_device().set_pin(epee::wipeable_string(pin.data(), pin.size()));
    } catch (const std::exception &e) {
        setStatusError(e.what());
    }
    return status() == Status_Ok;
}

bool WalletImpl::setDevicePassphrase(const std::string &passphrase)
{
    clearStatus();
    try {
        m_wallet->get_account().get_device().set_passphrase(epee::wipeable_string(passphrase.data(), passphrase.size()));
    } catch (const std::exception &e) {
        setStatusError(e.what());
    }
    return status() == Status_Ok;
}

std::string WalletImpl::address(uint32_t accountIndex, uint32_t addressIndex) const
{
    return m_wallet->get_subaddress_as_str({accountIndex, addressIndex});
}

std::string WalletImpl::integratedAddress(const std::string &payment_id) const
{
    crypto::hash8 pid;
    if (!tools::wallet2::parse_short_payment_id(payment_id, pid)) {
        return "";
    }
    return m_wallet->get_integrated_address_as_str(pid);
}

std::string WalletImpl::secretViewKey() const
{
    return epee::string_tools::pod_to_hex(unwrap(unwrap(m_wallet->get_account().get_keys().m_view_secret_key)));
}

std::string WalletImpl::publicViewKey() const
{
    return epee::string_tools::pod_to_hex(m_wallet->get_account().get_keys().m_account_address.m_view_public_key);
}

std::string WalletImpl::secretSpendKey() const
{
    return epee::string_tools::pod_to_hex(unwrap(unwrap(m_wallet->get_account().get_keys().m_spend_secret_key)));
}

std::string WalletImpl::publicSpendKey() const
{
    return epee::string_tools::pod_to_hex(m_wallet->get_account().get_keys().m_account_address.m_spend_public_key);
}

std::string WalletImpl::publicMultisigSignerKey() const
{
    try {
        crypto::public_key signer = m_wallet->get_multisig_signer_public_key();
        return epee::string_tools::pod_to_hex(signer);
    } catch (const std::exception&) {
        return "";
    }
}

std::string WalletImpl::path() const
{
    return m_wallet->path();
}

void WalletImpl::stop()
{
    m_wallet->stop();
}

bool WalletImpl::store(const std::string &path)
{
    clearStatus();
    try {
        if (path.empty()) {
            m_wallet->store();
        } else {
            m_wallet->store_to(path, m_password);
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Error saving wallet: " << e.what());
        setStatusError(e.what());
        return false;
    }

    return true;
}

string WalletImpl::filename() const
{
    return m_wallet->get_wallet_file();
}

string WalletImpl::keysFilename() const
{
    return m_wallet->get_keys_file();
}

bool WalletImpl::init(const std::string &daemon_address, uint64_t upper_transaction_size_limit, const std::string &daemon_username, const std::string &daemon_password, bool use_ssl, bool lightWallet, const std::string &proxy_address)
{
    clearStatus();
    if(daemon_username != "")
        m_daemon_login.emplace(daemon_username, daemon_password);
    return doInit(daemon_address, proxy_address, upper_transaction_size_limit, use_ssl);
}

void WalletImpl::setRefreshFromBlockHeight(uint64_t refresh_from_block_height)
{
    if (checkBackgroundSync("cannot change refresh height"))
        return;
    m_wallet->set_refresh_from_block_height(refresh_from_block_height);
}

void WalletImpl::setRecoveringFromSeed(bool recoveringFromSeed)
{
    m_recoveringFromSeed = recoveringFromSeed;
}

void WalletImpl::setRecoveringFromDevice(bool recoveringFromDevice)
{
    m_recoveringFromDevice = recoveringFromDevice;
}

void WalletImpl::setSubaddressLookahead(uint32_t major, uint32_t minor)
{
    m_wallet->set_subaddress_lookahead(major, minor);
}

uint64_t WalletImpl::balance(uint32_t accountIndex) const
{
    return m_wallet->balance(accountIndex, false);
}

uint64_t WalletImpl::unlockedBalance(uint32_t accountIndex) const
{
    return m_wallet->unlocked_balance(accountIndex, false);
}

uint64_t WalletImpl::blockChainHeight() const
{
    return m_wallet->get_blockchain_current_height();
}
uint64_t WalletImpl::approximateBlockChainHeight() const
{
    return m_wallet->get_approximate_blockchain_height();
}

uint64_t WalletImpl::estimateBlockChainHeight() const
{
    return m_wallet->estimate_blockchain_height();
}

uint64_t WalletImpl::daemonBlockChainHeight() const
{
    if (!m_is_connected)
        return 0;
    std::string err;
    uint64_t result = m_wallet->get_daemon_blockchain_height(err);
    if (!err.empty()) {
        LOG_ERROR(__FUNCTION__ << ": " << err);
        result = 0;
        setStatusError(err);
    } else {
        clearStatus();
    }
    return result;
}

uint64_t WalletImpl::daemonBlockChainTargetHeight() const
{
    if (!m_is_connected)
        return 0;
    std::string err;
    uint64_t result = m_wallet->get_daemon_blockchain_target_height(err);
    if (!err.empty()) {
        LOG_ERROR(__FUNCTION__ << ": " << err);
        result = 0;
        setStatusError(err);
    } else {
        clearStatus();
    }
    // Target height can be 0 when daemon is synced. Use blockchain height instead. 
    if(result == 0)
        result = daemonBlockChainHeight();
    return result;
}

bool WalletImpl::daemonSynced() const
{   
    if(connected() == Wallet::ConnectionStatus_Disconnected)
        return false;
    uint64_t blockChainHeight = daemonBlockChainHeight();
    return (blockChainHeight >= daemonBlockChainTargetHeight() && blockChainHeight > 1);
}

bool WalletImpl::synchronized() const
{
    return m_synchronized;
}

bool WalletImpl::refresh()
{
    clearStatus();
    doRefresh();
    return status() == Status_Ok;
}

void WalletImpl::refreshAsync()
{
    LOG_PRINT_L3(__FUNCTION__ << ": Refreshing asynchronously..");
    clearStatus();
    m_refreshCV.notify_one();
}

bool WalletImpl::rescanBlockchain()
{
    if (checkBackgroundSync("cannot rescan blockchain"))
        return false;
    clearStatus();
    m_refreshShouldRescan = true;
    doRefresh();
    return status() == Status_Ok;
}

void WalletImpl::rescanBlockchainAsync()
{
    if (checkBackgroundSync("cannot rescan blockchain"))
        return;
    m_refreshShouldRescan = true;
    refreshAsync();
}

void WalletImpl::setAutoRefreshInterval(int millis)
{
    if (millis > MAX_REFRESH_INTERVAL_MILLIS) {
        LOG_ERROR(__FUNCTION__<< ": invalid refresh interval " << millis
                  << " ms, maximum allowed is " << MAX_REFRESH_INTERVAL_MILLIS << " ms");
        m_refreshIntervalMillis = MAX_REFRESH_INTERVAL_MILLIS;
    } else {
        m_refreshIntervalMillis = millis;
    }
}

int WalletImpl::autoRefreshInterval() const
{
    return m_refreshIntervalMillis;
}

UnsignedTransaction *WalletImpl::loadUnsignedTx(const std::string &unsigned_filename) {
  clearStatus();
  UnsignedTransactionImpl * transaction = new UnsignedTransactionImpl(*this);
  if (checkBackgroundSync("cannot load tx") || !m_wallet->load_unsigned_tx(unsigned_filename, transaction->m_unsigned_tx_set)){
    setStatusError(tr("Failed to load unsigned transactions"));
    transaction->m_status = UnsignedTransaction::Status::Status_Error;
    transaction->m_errorString = errorString();

    return transaction;
  }
  
  // Check tx data and construct confirmation message
  std::string extra_message;
  if (!std::get<2>(transaction->m_unsigned_tx_set.transfers).empty())
    extra_message = (boost::format("%u outputs to import. ") % (unsigned)std::get<2>(transaction->m_unsigned_tx_set.transfers).size()).str();
  transaction->checkLoadedTx([&transaction](){return transaction->m_unsigned_tx_set.txes.size();}, [&transaction](size_t n)->const tools::wallet2::tx_construction_data&{return transaction->m_unsigned_tx_set.txes[n];}, extra_message);
  setStatus(transaction->status(), transaction->errorString());
    
  return transaction;
}

bool WalletImpl::submitTransaction(const string &fileName) {
  clearStatus();
  if (checkBackgroundSync("cannot submit tx"))
    return false;
  std::unique_ptr<PendingTransactionImpl> transaction(new PendingTransactionImpl(*this));

  // TODO : use WalletImpl::loadTx() if ready
  bool r = m_wallet->load_tx(fileName, transaction->m_pending_tx);
  if (!r) {
    setStatus(Status_Ok, tr("Failed to load transaction from file"));
    return false;
  }
  
  if(!transaction->commit()) {
    setStatusError(transaction->m_errorString);
    return false;
  }

  return true;
}

bool WalletImpl::exportKeyImages(const string &filename, bool all) 
{
  if (watchOnly())
  {
    setStatusError(tr("Wallet is view only"));
    return false;
  }
  if (checkBackgroundSync("cannot export key images"))
    return false;
  
  try
  {
    if (!m_wallet->export_key_images(filename, all))
    {
      setStatusError(tr("failed to save file ") + filename);
      return false;
    }
  }
  catch (const std::exception &e)
  {
    LOG_ERROR("Error exporting key images: " << e.what());
    setStatusError(e.what());
    return false;
  }
  return true;
}

bool WalletImpl::importKeyImages(const string &filename)
{
  if (checkBackgroundSync("cannot import key images"))
    return false;
  if (!trustedDaemon()) {
    setStatusError(tr("Key images can only be imported with a trusted daemon"));
    return false;
  }
  try
  {
    uint64_t spent = 0, unspent = 0;
    uint64_t height = m_wallet->import_key_images(filename, spent, unspent);
    LOG_PRINT_L2("Signed key images imported to height " << height << ", "
        << print_money(spent) << " spent, " << print_money(unspent) << " unspent");
  }
  catch (const std::exception &e)
  {
    LOG_ERROR("Error exporting key images: " << e.what());
    setStatusError(string(tr("Failed to import key images: ")) + e.what());
    return false;
  }

  return true;
}

bool WalletImpl::exportOutputs(const string &filename, bool all)
{
    if (checkBackgroundSync("cannot export outputs"))
        return false;
    if (m_wallet->key_on_device())
    {
        setStatusError(string(tr("Not supported on HW wallets.")) + filename);
        return false;
    }

    try
    {
        std::string data = exportOutputsToStr(all);
        bool r = saveToFile(filename, data);
        if (!r)
        {
            LOG_ERROR("Failed to save file " << filename);
            setStatusError(string(tr("Failed to save file: ")) + filename);
            return false;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Error exporting outputs: " << e.what());
        setStatusError(string(tr("Error exporting outputs: ")) + e.what());
        return false;
    }

    LOG_PRINT_L2("Outputs exported to " << filename);
    return true;
}

bool WalletImpl::importOutputs(const string &filename)
{
    if (checkBackgroundSync("cannot import outputs"))
        return false;
    if (m_wallet->key_on_device())
    {
        setStatusError(string(tr("Not supported on HW wallets.")) + filename);
        return false;
    }

    std::string data;
    bool r = loadFromFile(filename, data);
    if (!r)
    {
        LOG_ERROR("Failed to read file: " << filename);
        setStatusError(string(tr("Failed to read file: ")) + filename);
        return false;
    }

    try
    {
        size_t n_outputs = importOutputsFromStr(data);
        if (status() != Status_Ok)
            throw runtime_error(errorString());
        LOG_PRINT_L2(std::to_string(n_outputs) << " outputs imported");
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to import outputs: " << e.what());
        setStatusError(string(tr("Failed to import outputs: ")) + e.what());
        return false;
    }

    return true;
}

bool WalletImpl::scanTransactions(const std::vector<std::string> &txids)
{
    if (checkBackgroundSync("cannot scan transactions"))
        return false;
    if (txids.empty())
    {
        setStatusError(string(tr("Failed to scan transactions: no transaction ids provided.")));
        return false;
    }

    // Parse and dedup args
    std::unordered_set<crypto::hash> txids_u;
    for (const auto &s : txids)
    {
        crypto::hash txid;
        if (!epee::string_tools::hex_to_pod(s, txid))
        {
            setStatusError(string(tr("Invalid txid specified: ")) + s);
            return false;
        }
        txids_u.insert(txid);
    }

    try
    {
        m_wallet->scan_tx(txids_u);
    }
    catch (const tools::error::wont_reprocess_recent_txs_via_untrusted_daemon &e)
    {
        setStatusError(e.what());
        return false;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to scan transaction: " << e.what());
        setStatusError(string(tr("Failed to scan transaction: ")) + e.what());
        return false;
    }

    return true;
}

bool WalletImpl::setupBackgroundSync(const Wallet::BackgroundSyncType background_sync_type, const std::string &wallet_password, const optional<std::string> &background_cache_password)
{
    try
    {
        PRE_VALIDATE_BACKGROUND_SYNC();

        tools::wallet2::BackgroundSyncType bgs_type;
        switch (background_sync_type)
        {
            case Wallet::BackgroundSync_Off: bgs_type = tools::wallet2::BackgroundSyncOff; break;
            case Wallet::BackgroundSync_ReusePassword: bgs_type = tools::wallet2::BackgroundSyncReusePassword; break;
            case Wallet::BackgroundSync_CustomPassword: bgs_type = tools::wallet2::BackgroundSyncCustomPassword; break;
            default: setStatusError(tr("Unknown background sync type")); return false;
        }

        boost::optional<epee::wipeable_string> bgc_password = background_cache_password
            ? boost::optional<epee::wipeable_string>(*background_cache_password)
            : boost::none;

        LOCK_REFRESH();
        m_wallet->setup_background_sync(bgs_type, wallet_password, bgc_password);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to setup background sync: " << e.what());
        setStatusError(string(tr("Failed to setup background sync: ")) + e.what());
        return false;
    }
    return true;
}

Wallet::BackgroundSyncType WalletImpl::getBackgroundSyncType() const
{
    switch (m_wallet->background_sync_type())
    {
        case tools::wallet2::BackgroundSyncOff: return Wallet::BackgroundSync_Off;
        case tools::wallet2::BackgroundSyncReusePassword: return Wallet::BackgroundSync_ReusePassword;
        case tools::wallet2::BackgroundSyncCustomPassword: return Wallet::BackgroundSync_CustomPassword;
        default: setStatusError(tr("Unknown background sync type")); return Wallet::BackgroundSync_Off;
    }
}

bool WalletImpl::startBackgroundSync()
{
    try
    {
        PRE_VALIDATE_BACKGROUND_SYNC();
        LOCK_REFRESH();
        m_wallet->start_background_sync();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to start background sync: " << e.what());
        setStatusError(string(tr("Failed to start background sync: ")) + e.what());
        return false;
    }
    return true;
}

bool WalletImpl::stopBackgroundSync(const std::string &wallet_password)
{
    try
    {
        PRE_VALIDATE_BACKGROUND_SYNC();
        LOCK_REFRESH();
        m_wallet->stop_background_sync(epee::wipeable_string(wallet_password));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to stop background sync: " << e.what());
        setStatusError(string(tr("Failed to stop background sync: ")) + e.what());
        return false;
    }
    return true;
}

void WalletImpl::addSubaddressAccount(const std::string& label)
{
    if (checkBackgroundSync("cannot add account"))
        return;
    m_wallet->add_subaddress_account(label);
}
size_t WalletImpl::numSubaddressAccounts() const
{
    return m_wallet->get_num_subaddress_accounts();
}
size_t WalletImpl::numSubaddresses(uint32_t accountIndex) const
{
    return m_wallet->get_num_subaddresses(accountIndex);
}
void WalletImpl::addSubaddress(uint32_t accountIndex, const std::string& label)
{
    if (checkBackgroundSync("cannot add subbaddress"))
        return;
    m_wallet->add_subaddress(accountIndex, label);
}
std::string WalletImpl::getSubaddressLabel(uint32_t accountIndex, uint32_t addressIndex) const
{
    if (checkBackgroundSync("cannot get subbaddress label"))
        return "";
    try
    {
        return m_wallet->get_subaddress_label({accountIndex, addressIndex});
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Error getting subaddress label: " << e.what());
        setStatusError(string(tr("Failed to get subaddress label: ")) + e.what());
        return "";
    }
}
void WalletImpl::setSubaddressLabel(uint32_t accountIndex, uint32_t addressIndex, const std::string &label)
{
    if (checkBackgroundSync("cannot set subbaddress label"))
        return;
    try
    {
        return m_wallet->set_subaddress_label({accountIndex, addressIndex}, label);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Error setting subaddress label: " << e.what());
        setStatusError(string(tr("Failed to set subaddress label: ")) + e.what());
    }
}

MultisigState WalletImpl::multisig() const {
    MultisigState state;
    if (checkBackgroundSync("cannot use multisig"))
        return state;

    const multisig::multisig_account_status ms_status{m_wallet->get_multisig_status()};

    state.isMultisig = ms_status.multisig_is_active;
    state.kexIsDone = ms_status.kex_is_done;
    state.isReady = ms_status.is_ready;
    state.threshold = ms_status.threshold;
    state.total = ms_status.total;

    return state;
}

string WalletImpl::getMultisigInfo() const {
    if (checkBackgroundSync("cannot use multisig"))
        return string();
    try {
        clearStatus();
        return m_wallet->get_multisig_first_kex_msg();
    } catch (const exception& e) {
        LOG_ERROR("Error on generating multisig info: " << e.what());
        setStatusError(string(tr("Failed to get multisig info: ")) + e.what());
    }

    return string();
}

string WalletImpl::makeMultisig(const vector<string>& info, const uint32_t threshold) {
    if (checkBackgroundSync("cannot make multisig"))
        return string();
    try {
        clearStatus();

        if (multisig().isMultisig) {
            throw runtime_error("Wallet is already multisig");
        }

        return m_wallet->make_multisig(epee::wipeable_string(m_password), info, threshold);
    } catch (const exception& e) {
        LOG_ERROR("Error on making multisig wallet: " << e.what());
        setStatusError(string(tr("Failed to make multisig: ")) + e.what());
    }

    return string();
}

std::string WalletImpl::exchangeMultisigKeys(const std::vector<std::string> &info, const bool force_update_use_with_caution /*= false*/) {
    try {
        clearStatus();
        checkMultisigWalletNotReady(m_wallet);

        return m_wallet->exchange_multisig_keys(epee::wipeable_string(m_password), info, force_update_use_with_caution);
    } catch (const exception& e) {
        LOG_ERROR("Error on exchanging multisig keys: " << e.what());
        setStatusError(string(tr("Failed to exchange multisig keys: ")) + e.what());
    }

    return string();
}

std::string WalletImpl::getMultisigKeyExchangeBooster(const std::vector<std::string> &info,
    const std::uint32_t threshold,
    const std::uint32_t num_signers) {
    try {
        clearStatus();

        return m_wallet->get_multisig_key_exchange_booster(epee::wipeable_string(m_password), info, threshold, num_signers);
    } catch (const exception& e) {
        LOG_ERROR("Error on boosting multisig key exchange: " << e.what());
        setStatusError(string(tr("Failed to boost multisig key exchange: ")) + e.what());
    }

    return string();
}

bool WalletImpl::exportMultisigImages(string& images) {
    try {
        clearStatus();
        checkMultisigWalletReady(m_wallet);

        auto blob = m_wallet->export_multisig();
        images = epee::string_tools::buff_to_hex_nodelimer(blob);
        return true;
    } catch (const exception& e) {
        LOG_ERROR("Error on exporting multisig images: " << e.what());
        setStatusError(string(tr("Failed to export multisig images: ")) + e.what());
    }

    return false;
}

size_t WalletImpl::importMultisigImages(const vector<string>& images) {
    try {
        clearStatus();
        checkMultisigWalletReady(m_wallet);

        std::vector<std::string> blobs;
        blobs.reserve(images.size());

        for (const auto& image: images) {
            std::string blob;
            if (!epee::string_tools::parse_hexstr_to_binbuff(image, blob)) {
                LOG_ERROR("Failed to parse imported multisig images");
                setStatusError(tr("Failed to parse imported multisig images"));
                return 0;
            }

            blobs.emplace_back(std::move(blob));
        }

        return m_wallet->import_multisig(blobs);
    } catch (const exception& e) {
        LOG_ERROR("Error on importing multisig images: " << e.what());
        setStatusError(string(tr("Failed to import multisig images: ")) + e.what());
    }

    return 0;
}

bool WalletImpl::hasMultisigPartialKeyImages() const {
    try {
        clearStatus();
        checkMultisigWalletReady(m_wallet);

        return m_wallet->has_multisig_partial_key_images();
    } catch (const exception& e) {
        LOG_ERROR("Error on checking for partial multisig key images: " << e.what());
        setStatusError(string(tr("Failed to check for partial multisig key images: ")) + e.what());
    }

    return false;
}

PendingTransaction* WalletImpl::restoreMultisigTransaction(const string& signData) {
    try {
        clearStatus();
        checkMultisigWalletReady(m_wallet);

        string binary;
        if (!epee::string_tools::parse_hexstr_to_binbuff(signData, binary)) {
            throw runtime_error("Failed to deserialize multisig transaction");
        }

        tools::wallet2::multisig_tx_set txSet;
        if (!m_wallet->load_multisig_tx(binary, txSet, {})) {
          throw runtime_error("couldn't parse multisig transaction data");
        }

        auto ptx = new PendingTransactionImpl(*this);
        ptx->m_pending_tx = txSet.m_ptx;
        ptx->m_signers = txSet.m_signers;

        return ptx;
    } catch (exception& e) {
        LOG_ERROR("Error on restoring multisig transaction: " << e.what());
        setStatusError(string(tr("Failed to restore multisig transaction: ")) + e.what());
    }

    return nullptr;
}

// TODO:
// 1 - properly handle payment id (add another menthod with explicit 'payment_id' param)
// 2 - check / design how "Transaction" can be single interface
// (instead of few different data structures within wallet2 implementation:
//    - pending_tx;
//    - transfer_details;
//    - payment_details;
//    - unconfirmed_transfer_details;
//    - confirmed_transfer_details)

PendingTransaction *WalletImpl::createTransactionMultDest(const std::vector<string> &dst_addr, const string &payment_id, optional<std::vector<uint64_t>> amount, uint32_t mixin_count, PendingTransaction::Priority priority, uint32_t subaddr_account, std::set<uint32_t> subaddr_indices)

{
    clearStatus();
    // Pause refresh thread while creating transaction
    pauseRefresh();
      
    cryptonote::address_parse_info info;

    PendingTransactionImpl * transaction = new PendingTransactionImpl(*this);

    uint32_t adjusted_priority = adjustPriority(static_cast<uint32_t>(priority));
    // TODO : Consider adding a function that returns `status() == Status_Ok`:
//    if (!isStatusOk())
    if (status() != Status_Ok)
    {
        return transaction;
    }

    do {
        if (checkBackgroundSync("cannot create transactions"))
            break;

        std::vector<uint8_t> extra;
        std::string extra_nonce;
        vector<cryptonote::tx_destination_entry> dsts;
        if (!amount && dst_addr.size() > 1) {
            setStatusError(tr("Sending all requires one destination address"));
            break;
        }
        if (amount && (dst_addr.size() != (*amount).size())) {
            setStatusError(tr("Destinations and amounts are unequal"));
            break;
        }
        if (!payment_id.empty()) {
            crypto::hash payment_id_long;
            if (tools::wallet2::parse_long_payment_id(payment_id, payment_id_long)) {
                cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_long);
            } else {
                setStatusError(tr("payment id has invalid format, expected 64 character hex string: ") + payment_id);
                break;
            }
        }
        bool error = false;
        for (size_t i = 0; i < dst_addr.size() && !error; i++) {
            if(!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), dst_addr[i])) {
                // TODO: copy-paste 'if treating as an address fails, try as url' from simplewallet.cpp:1982
                setStatusError(tr("Invalid destination address"));
                error = true;
                break;
            }
            if (info.has_payment_id) {
                if (!extra_nonce.empty()) {
                    setStatusError(tr("a single transaction cannot use more than one payment id"));
                    error = true;
                    break;
                }
                set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, info.payment_id);
            }

            if (amount) {
                cryptonote::tx_destination_entry de;
                de.original = dst_addr[i];
                de.addr = info.address;
                de.amount = (*amount)[i];
                de.is_subaddress = info.is_subaddress;
                de.is_integrated = info.has_payment_id;
                dsts.push_back(de);
            } else {
                if (subaddr_indices.empty()) {
                    for (uint32_t index = 0; index < numSubaddresses(subaddr_account); ++index)
                        subaddr_indices.insert(index);
                }
            }
        }
        if (error) {
            break;
        }
        if (!extra_nonce.empty() && !add_extra_nonce_to_tx_extra(extra, extra_nonce)) {
            setStatusError(tr("failed to set up payment id, though it was decoded correctly"));
            break;
        }
        try {
            size_t fake_outs_count = mixin_count > 0 ? mixin_count : defaultMixin();
            fake_outs_count = adjustMixin(mixin_count);

            if (amount) {
                transaction->m_pending_tx = m_wallet->create_transactions_2(dsts, fake_outs_count,
                                                                            adjusted_priority,
                                                                            extra, subaddr_account, subaddr_indices);
            } else {
                transaction->m_pending_tx = m_wallet->create_transactions_all(0, info.address, info.is_subaddress, 1, fake_outs_count,
                                                                              adjusted_priority,
                                                                              extra, subaddr_account, subaddr_indices);
            }
            pendingTxPostProcess(transaction);

            if (multisig().isMultisig) {
                makeMultisigTxSet(transaction);
            }
        } catch (const tools::error::daemon_busy&) {
            // TODO: make it translatable with "tr"?
            setStatusError(tr("daemon is busy. Please try again later."));
        } catch (const tools::error::no_connection_to_daemon&) {
            setStatusError(tr("no connection to daemon. Please make sure daemon is running."));
        } catch (const tools::error::wallet_rpc_error& e) {
            setStatusError(tr("RPC error: ") +  e.to_string());
        } catch (const tools::error::get_outs_error &e) {
            setStatusError((boost::format(tr("failed to get outputs to mix: %s")) % e.what()).str());
        } catch (const tools::error::not_enough_unlocked_money& e) {
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, available only %s, sent amount %s")) %
                      print_money(e.available()) %
                      print_money(e.tx_amount());
            setStatusError(writer.str());
        } catch (const tools::error::not_enough_money& e) {
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, overall balance only %s, sent amount %s")) %
                      print_money(e.available()) %
                      print_money(e.tx_amount());
            setStatusError(writer.str());
        } catch (const tools::error::tx_not_possible& e) {
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, available only %s, transaction amount %s = %s + %s (fee)")) %
                      print_money(e.available()) %
                      print_money(e.tx_amount() + e.fee())  %
                      print_money(e.tx_amount()) %
                      print_money(e.fee());
            setStatusError(writer.str());
        } catch (const tools::error::not_enough_outs_to_mix& e) {
            std::ostringstream writer;
            writer << tr("not enough outputs for specified ring size") << " = " << (e.mixin_count() + 1) << ":";
            for (const std::pair<uint64_t, uint64_t> outs_for_amount : e.scanty_outs()) {
                writer << "\n" << tr("output amount") << " = " << print_money(outs_for_amount.first) << ", " << tr("found outputs to use") << " = " << outs_for_amount.second;
            }
            writer << "\n" << tr("Please sweep unmixable outputs.");
            setStatusError(writer.str());
        } catch (const tools::error::tx_not_constructed&) {
            setStatusError(tr("transaction was not constructed"));
        } catch (const tools::error::tx_rejected& e) {
            std::ostringstream writer;
            writer << (boost::format(tr("transaction %s was rejected by daemon with status: ")) % get_transaction_hash(e.tx())) <<  e.status();
            setStatusError(writer.str());
        } catch (const tools::error::tx_sum_overflow& e) {
            setStatusError(e.what());
        } catch (const tools::error::zero_amount&) {
            setStatusError(tr("destination amount is zero"));
        } catch (const tools::error::zero_destination&) {
            setStatusError(tr("transaction has no destination"));
        } catch (const tools::error::tx_too_big& e) {
            setStatusError(tr("failed to find a suitable way to split transactions"));
        } catch (const tools::error::transfer_error& e) {
            setStatusError(string(tr("unknown transfer error: ")) + e.what());
        } catch (const tools::error::wallet_internal_error& e) {
            setStatusError(string(tr("internal error: ")) + e.what());
        } catch (const std::exception& e) {
            setStatusError(string(tr("unexpected error: ")) + e.what());
        } catch (...) {
            setStatusError(tr("unknown error"));
        }
    } while (false);

    statusWithErrorString(transaction->m_status, transaction->m_errorString);
    // Resume refresh thread
    startRefresh();
    return transaction;
}

PendingTransaction *WalletImpl::createTransaction(const string &dst_addr, const string &payment_id, optional<uint64_t> amount, uint32_t mixin_count,
                                                  PendingTransaction::Priority priority, uint32_t subaddr_account, std::set<uint32_t> subaddr_indices)

{
    return createTransactionMultDest(std::vector<string> {dst_addr}, payment_id, amount ? (std::vector<uint64_t> {*amount}) : (optional<std::vector<uint64_t>>()), mixin_count, priority, subaddr_account, subaddr_indices);
}

PendingTransaction *WalletImpl::createSweepUnmixableTransaction()

{
    clearStatus();
    cryptonote::tx_destination_entry de;

    PendingTransactionImpl * transaction = new PendingTransactionImpl(*this);

    do {
        if (checkBackgroundSync("cannot sweep"))
            break;

        try {
            transaction->m_pending_tx = m_wallet->create_unmixable_sweep_transactions();
            pendingTxPostProcess(transaction);

        } catch (const tools::error::daemon_busy&) {
            // TODO: make it translatable with "tr"?
            setStatusError(tr("daemon is busy. Please try again later."));
        } catch (const tools::error::no_connection_to_daemon&) {
            setStatusError(tr("no connection to daemon. Please make sure daemon is running."));
        } catch (const tools::error::wallet_rpc_error& e) {
            setStatusError(tr("RPC error: ") +  e.to_string());
        } catch (const tools::error::get_outs_error&) {
            setStatusError(tr("failed to get outputs to mix"));
        } catch (const tools::error::not_enough_unlocked_money& e) {
            setStatusError("");
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, available only %s, sent amount %s")) %
                      print_money(e.available()) %
                      print_money(e.tx_amount());
            setStatusError(writer.str());
        } catch (const tools::error::not_enough_money& e) {
            setStatusError("");
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, overall balance only %s, sent amount %s")) %
                      print_money(e.available()) %
                      print_money(e.tx_amount());
            setStatusError(writer.str());
        } catch (const tools::error::tx_not_possible& e) {
            setStatusError("");
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, available only %s, transaction amount %s = %s + %s (fee)")) %
                      print_money(e.available()) %
                      print_money(e.tx_amount() + e.fee())  %
                      print_money(e.tx_amount()) %
                      print_money(e.fee());
            setStatusError(writer.str());
        } catch (const tools::error::not_enough_outs_to_mix& e) {
            std::ostringstream writer;
            writer << tr("not enough outputs for specified ring size") << " = " << (e.mixin_count() + 1) << ":";
            for (const std::pair<uint64_t, uint64_t> outs_for_amount : e.scanty_outs()) {
                writer << "\n" << tr("output amount") << " = " << print_money(outs_for_amount.first) << ", " << tr("found outputs to use") << " = " << outs_for_amount.second;
            }
            setStatusError(writer.str());
        } catch (const tools::error::tx_not_constructed&) {
            setStatusError(tr("transaction was not constructed"));
        } catch (const tools::error::tx_rejected& e) {
            std::ostringstream writer;
            writer << (boost::format(tr("transaction %s was rejected by daemon with status: ")) % get_transaction_hash(e.tx())) <<  e.status();
            setStatusError(writer.str());
        } catch (const tools::error::tx_sum_overflow& e) {
            setStatusError(e.what());
        } catch (const tools::error::zero_destination&) {
            setStatusError(tr("one of destinations is zero"));
        } catch (const tools::error::tx_too_big& e) {
            setStatusError(tr("failed to find a suitable way to split transactions"));
        } catch (const tools::error::transfer_error& e) {
            setStatusError(string(tr("unknown transfer error: ")) + e.what());
        } catch (const tools::error::wallet_internal_error& e) {
            setStatusError(string(tr("internal error: ")) + e.what());
        } catch (const std::exception& e) {
            setStatusError(string(tr("unexpected error: ")) + e.what());
        } catch (...) {
            setStatusError(tr("unknown error"));
        }
    } while (false);

    statusWithErrorString(transaction->m_status, transaction->m_errorString);
    return transaction;
}

void WalletImpl::disposeTransaction(PendingTransaction *t)
{
    delete t;
}

uint64_t WalletImpl::estimateTransactionFee(const std::vector<std::pair<std::string, uint64_t>> &destinations,
                                            PendingTransaction::Priority priority) const
{
    const size_t pubkey_size = 33;
    const size_t encrypted_paymentid_size = 11;
    const size_t extra_size = pubkey_size + encrypted_paymentid_size;

    return m_wallet->estimate_fee(
        useForkRules(HF_VERSION_PER_BYTE_FEE, 0),
        useForkRules(4, 0),
        1,
        getMinRingSize() - 1,
        destinations.size() + 1,
        extra_size,
        useForkRules(8, 0),
        useForkRules(HF_VERSION_CLSAG, 0),
        useForkRules(HF_VERSION_BULLETPROOF_PLUS, 0),
        useForkRules(HF_VERSION_VIEW_TAGS, 0),
        getBaseFee(priority),
        m_wallet->get_fee_quantization_mask());
}

TransactionHistory *WalletImpl::history()
{
    return m_history.get();
}

AddressBook *WalletImpl::addressBook()
{
    return m_addressBook.get();
}

Subaddress *WalletImpl::subaddress()
{
    return m_subaddress.get();
}

SubaddressAccount *WalletImpl::subaddressAccount()
{
    return m_subaddressAccount.get();
}

void WalletImpl::setListener(WalletListener *l)
{
    // TODO thread synchronization;
    m_wallet2Callback->setListener(l);
}

uint32_t WalletImpl::defaultMixin() const
{
    return m_wallet->default_mixin();
}

void WalletImpl::setDefaultMixin(uint32_t arg)
{
    if (checkBackgroundSync("cannot set default mixin"))
        return;
    m_wallet->default_mixin(arg);
}

bool WalletImpl::setCacheAttribute(const std::string &key, const std::string &val)
{
    if (checkBackgroundSync("cannot set cache attribute"))
        return false;
    m_wallet->set_attribute(key, val);
    return true;
}

std::string WalletImpl::getCacheAttribute(const std::string &key) const
{
    std::string value;
    m_wallet->get_attribute(key, value);
    return value;
}

bool WalletImpl::setUserNote(const std::string &txid, const std::string &note)
{
    if (checkBackgroundSync("cannot set user note"))
        return false;
    cryptonote::blobdata txid_data;
    if(!epee::string_tools::parse_hexstr_to_binbuff(txid, txid_data) || txid_data.size() != sizeof(crypto::hash))
      return false;
    const crypto::hash htxid = *reinterpret_cast<const crypto::hash*>(txid_data.data());

    m_wallet->set_tx_note(htxid, note);
    return true;
}

std::string WalletImpl::getUserNote(const std::string &txid) const
{
    if (checkBackgroundSync("cannot get user note"))
        return "";
    cryptonote::blobdata txid_data;
    if(!epee::string_tools::parse_hexstr_to_binbuff(txid, txid_data) || txid_data.size() != sizeof(crypto::hash))
      return "";
    const crypto::hash htxid = *reinterpret_cast<const crypto::hash*>(txid_data.data());

    return m_wallet->get_tx_note(htxid);
}

std::string WalletImpl::getTxKey(const std::string &txid_str) const
{
    if (checkBackgroundSync("cannot get tx key"))
        return "";

    crypto::hash txid;
    if(!epee::string_tools::hex_to_pod(txid_str, txid))
    {
        setStatusError(tr("Failed to parse txid"));
        return "";
    }

    crypto::secret_key tx_key;
    std::vector<crypto::secret_key> additional_tx_keys;
    try
    {
        clearStatus();
        if (m_wallet->get_tx_key(txid, tx_key, additional_tx_keys))
        {
            clearStatus();
            std::ostringstream oss;
            oss << epee::string_tools::pod_to_hex(unwrap(unwrap(tx_key)));
            for (size_t i = 0; i < additional_tx_keys.size(); ++i)
                oss << epee::string_tools::pod_to_hex(unwrap(unwrap(additional_tx_keys[i])));
            return oss.str();
        }
        else
        {
            setStatusError(tr("no tx keys found for this txid"));
            return "";
        }
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return "";
    }
}

bool WalletImpl::checkTxKey(const std::string &txid_str, std::string tx_key_str, const std::string &address_str, uint64_t &received, bool &in_pool, uint64_t &confirmations)
{
    crypto::hash txid;
    if (!epee::string_tools::hex_to_pod(txid_str, txid))
    {
        setStatusError(tr("Failed to parse txid"));
        return false;
    }

    crypto::secret_key tx_key;
    std::vector<crypto::secret_key> additional_tx_keys;
    if (!epee::string_tools::hex_to_pod(tx_key_str.substr(0, 64), tx_key))
    {
        setStatusError(tr("Failed to parse tx key"));
        return false;
    }
    tx_key_str = tx_key_str.substr(64);
    while (!tx_key_str.empty())
    {
        additional_tx_keys.resize(additional_tx_keys.size() + 1);
        if (!epee::string_tools::hex_to_pod(tx_key_str.substr(0, 64), additional_tx_keys.back()))
        {
            setStatusError(tr("Failed to parse tx key"));
            return false;
        }
        tx_key_str = tx_key_str.substr(64);
    }

    cryptonote::address_parse_info info;
    if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), address_str))
    {
        setStatusError(tr("Failed to parse address"));
        return false;
    }

    try
    {
        m_wallet->check_tx_key(txid, tx_key, additional_tx_keys, info.address, received, in_pool, confirmations);
        clearStatus();
        return true;
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return false;
    }
}

std::string WalletImpl::getTxProof(const std::string &txid_str, const std::string &address_str, const std::string &message) const
{
    if (checkBackgroundSync("cannot get tx proof"))
        return "";

    crypto::hash txid;
    if (!epee::string_tools::hex_to_pod(txid_str, txid))
    {
        setStatusError(tr("Failed to parse txid"));
        return "";
    }

    cryptonote::address_parse_info info;
    if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), address_str))
    {
        setStatusError(tr("Failed to parse address"));
        return "";
    }

    try
    {
        clearStatus();
        return m_wallet->get_tx_proof(txid, info.address, info.is_subaddress, message);
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return "";
    }
}

bool WalletImpl::checkTxProof(const std::string &txid_str, const std::string &address_str, const std::string &message, const std::string &signature, bool &good, uint64_t &received, bool &in_pool, uint64_t &confirmations)
{
    crypto::hash txid;
    if (!epee::string_tools::hex_to_pod(txid_str, txid))
    {
        setStatusError(tr("Failed to parse txid"));
        return false;
    }

    cryptonote::address_parse_info info;
    if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), address_str))
    {
        setStatusError(tr("Failed to parse address"));
        return false;
    }

    try
    {
        good = m_wallet->check_tx_proof(txid, info.address, info.is_subaddress, message, signature, received, in_pool, confirmations);
        clearStatus();
        return true;
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return false;
    }
}

std::string WalletImpl::getSpendProof(const std::string &txid_str, const std::string &message) const {
    if (checkBackgroundSync("cannot get spend proof"))
        return "";

    crypto::hash txid;
    if(!epee::string_tools::hex_to_pod(txid_str, txid))
    {
        setStatusError(tr("Failed to parse txid"));
        return "";
    }

    try
    {
        clearStatus();
        return m_wallet->get_spend_proof(txid, message);
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return "";
    }
}

bool WalletImpl::checkSpendProof(const std::string &txid_str, const std::string &message, const std::string &signature, bool &good) const {
    good = false;
    crypto::hash txid;
    if(!epee::string_tools::hex_to_pod(txid_str, txid))
    {
        setStatusError(tr("Failed to parse txid"));
        return false;
    }

    try
    {
        clearStatus();
        good = m_wallet->check_spend_proof(txid, message, signature);
        return true;
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return false;
    }
}

std::string WalletImpl::getReserveProof(bool all, uint32_t account_index, uint64_t amount, const std::string &message) const {
    if (checkBackgroundSync("cannot get reserve proof"))
        return "";

    try
    {
        clearStatus();
        boost::optional<std::pair<uint32_t, uint64_t>> account_minreserve;
        if (!all)
        {
            account_minreserve = std::make_pair(account_index, amount);
        }
        return m_wallet->get_reserve_proof(account_minreserve, message);
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return "";
    }
}

bool WalletImpl::checkReserveProof(const std::string &address, const std::string &message, const std::string &signature, bool &good, uint64_t &total, uint64_t &spent) const {
    cryptonote::address_parse_info info;
    if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), address))
    {
        setStatusError(tr("Failed to parse address"));
        return false;
    }
    if (info.is_subaddress)
    {
        setStatusError(tr("Address must not be a subaddress"));
        return false;
    }

    good = false;
    try
    {
        clearStatus();
        good = m_wallet->check_reserve_proof(info.address, message, signature, total, spent);
        return true;
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
        return false;
    }
}

std::string WalletImpl::signMessage(const std::string &message, const std::string &address, bool sign_with_view_key)
{
    if (checkBackgroundSync("cannot sign message"))
        return "";

    // QUESTION : Can we get rid of this `clearStatus()`, because it's already called in `checkBackgroundSync()`, or should we leave this redundant call (I'd actually move it up to the top) so it's directly obvious that this `signMessage()` sets status error?
    clearStatus();

    tools::wallet2::message_signature_type_t sig_type = sign_with_view_key ? tools::wallet2::sign_with_view_key : tools::wallet2::sign_with_spend_key;

    if (address.empty()) {
        return m_wallet->sign(message, sig_type);
    }

    cryptonote::address_parse_info info;
    if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), address)) {
        setStatusError(tr("Failed to parse address"));
        return "";
    }
    auto index = m_wallet->get_subaddress_index(info.address);
    if (!index) {
        setStatusError(tr("Address doesn't belong to the wallet"));
        return "";
    }

    return m_wallet->sign(message, sig_type, *index);
}

bool WalletImpl::verifySignedMessage(const std::string &message, const std::string &address, const std::string &signature) const
{
  cryptonote::address_parse_info info;

  if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), address))
    return false;

  return m_wallet->verify(message, info.address, signature).valid;
}

std::string WalletImpl::signMultisigParticipant(const std::string &message) const
{
    clearStatus();

    const multisig::multisig_account_status ms_status{m_wallet->get_multisig_status()};
    if (!ms_status.multisig_is_active || !ms_status.is_ready) {
        m_status = Status_Error;
        m_errorString = tr("The wallet must be in multisig ready state");
        return {};
    }

    try {
        return m_wallet->sign_multisig_participant(message);
    } catch (const std::exception& e) {
        m_status = Status_Error;
        m_errorString = e.what();
    }

    return {};
}

bool WalletImpl::verifyMessageWithPublicKey(const std::string &message, const std::string &publicKey, const std::string &signature) const
{
    clearStatus();

    cryptonote::blobdata pkeyData;
    if(!epee::string_tools::parse_hexstr_to_binbuff(publicKey, pkeyData) || pkeyData.size() != sizeof(crypto::public_key))
    {
        m_status = Status_Error;
        m_errorString = tr("Given string is not a key");
        return false;
    }

    try {
        crypto::public_key pkey = *reinterpret_cast<const crypto::public_key*>(pkeyData.data());
        return m_wallet->verify_with_public_key(message, pkey, signature);
    } catch (const std::exception& e) {
        m_status = Status_Error;
        m_errorString = e.what();
    }

    return false;
}

bool WalletImpl::connectToDaemon()
{
    bool result = m_wallet->check_connection(NULL, NULL, DEFAULT_CONNECTION_TIMEOUT_MILLIS);
    if (!result) {
        setStatusError("Error connecting to daemon at " + m_wallet->get_daemon_address());
    } else {
        clearStatus();
        // start refreshing here
    }
    return result;
}

Wallet::ConnectionStatus WalletImpl::connected() const
{
    uint32_t version = 0;
    bool wallet_is_outdated = false, daemon_is_outdated = false;
    m_is_connected = m_wallet->check_connection(&version, NULL, DEFAULT_CONNECTION_TIMEOUT_MILLIS, &wallet_is_outdated, &daemon_is_outdated);
    if (!m_is_connected)
    {
        if (wallet_is_outdated || daemon_is_outdated)
            return Wallet::ConnectionStatus_WrongVersion;
        else
            return Wallet::ConnectionStatus_Disconnected;
    }
    if ((version >> 16) != CORE_RPC_VERSION_MAJOR)
        return Wallet::ConnectionStatus_WrongVersion;
    return Wallet::ConnectionStatus_Connected;
}

void WalletImpl::setTrustedDaemon(bool arg)
{
    m_wallet->set_trusted_daemon(arg);
}

bool WalletImpl::trustedDaemon() const
{
    return m_wallet->is_trusted_daemon();
}

bool WalletImpl::setProxy(const std::string &address)
{
    return m_wallet->set_proxy(address);
}

bool WalletImpl::watchOnly() const
{
    return m_wallet->watch_only();
}

bool WalletImpl::isDeterministic() const
{
    return m_wallet->is_deterministic();
}

bool WalletImpl::isBackgroundSyncing() const
{
    return m_wallet->is_background_syncing();
}

bool WalletImpl::isBackgroundWallet() const
{
    return m_wallet->is_background_wallet();
}

void WalletImpl::clearStatus() const
{
    boost::lock_guard<boost::mutex> l(m_statusMutex);
    m_status = Status_Ok;
    m_errorString.clear();
}

void WalletImpl::setStatusError(const std::string& message) const
{
    setStatus(Status_Error, message);
}

void WalletImpl::setStatusCritical(const std::string& message) const
{
    setStatus(Status_Critical, message);
}

void WalletImpl::setStatus(int status, const std::string& message) const
{
    boost::lock_guard<boost::mutex> l(m_statusMutex);
    m_status = status;
    m_errorString = message;
}

void WalletImpl::refreshThreadFunc()
{
    LOG_PRINT_L3(__FUNCTION__ << ": starting refresh thread");

    while (true) {
        boost::mutex::scoped_lock lock(m_refreshMutex);
        if (m_refreshThreadDone) {
            break;
        }
        LOG_PRINT_L3(__FUNCTION__ << ": waiting for refresh...");
        // if auto refresh enabled, we wait for the "m_refreshIntervalSeconds" interval.
        // if not - we wait forever
        if (m_refreshIntervalMillis > 0) {
            boost::posix_time::milliseconds wait_for_ms(m_refreshIntervalMillis.load());
            m_refreshCV.timed_wait(lock, wait_for_ms);
        } else {
            m_refreshCV.wait(lock);
        }

        LOG_PRINT_L3(__FUNCTION__ << ": refresh lock acquired...");
        LOG_PRINT_L3(__FUNCTION__ << ": m_refreshEnabled: " << m_refreshEnabled);
        LOG_PRINT_L3(__FUNCTION__ << ": m_status: " << status());
        LOG_PRINT_L3(__FUNCTION__ << ": m_refreshShouldRescan: " << m_refreshShouldRescan);
        if (m_refreshEnabled) {
            LOG_PRINT_L3(__FUNCTION__ << ": refreshing...");
            doRefresh();
        }
    }
    LOG_PRINT_L3(__FUNCTION__ << ": refresh thread stopped");
}

void WalletImpl::doRefresh()
{
    bool rescan = m_refreshShouldRescan.exchange(false);
    // synchronizing async and sync refresh calls
    boost::lock_guard<boost::mutex> guarg(m_refreshMutex2);
    do try {
        LOG_PRINT_L3(__FUNCTION__ << ": doRefresh, rescan = "<<rescan);
        // Syncing daemon and refreshing wallet simultaneously is very resource intensive.
        // Disable refresh if wallet is disconnected or daemon isn't synced.
        if (daemonSynced()) {
            if(rescan)
                m_wallet->rescan_blockchain(false);
            m_wallet->refresh(trustedDaemon());
            m_synchronized = m_wallet->is_synced();
            // assuming if we have empty history, it wasn't initialized yet
            // for further history changes client need to update history in
            // "on_money_received" and "on_money_sent" callbacks
            if (m_history->count() == 0) {
                m_history->refresh();
            }
            m_wallet->find_and_save_rings(false);
        } else {
           LOG_PRINT_L3(__FUNCTION__ << ": skipping refresh - daemon is not synced");
        }
    } catch (const std::exception &e) {
        setStatusError(e.what());
        break;
    }while(!rescan && (rescan=m_refreshShouldRescan.exchange(false))); // repeat if not rescanned and rescan was requested

    if (m_wallet2Callback->getListener()) {
        m_wallet2Callback->getListener()->refreshed();
    }
}


void WalletImpl::startRefresh()
{
    if (!m_refreshEnabled) {
        LOG_PRINT_L2(__FUNCTION__ << ": refresh started/resumed...");
        m_refreshEnabled = true;
        m_refreshCV.notify_one();
    }
}



void WalletImpl::stopRefresh()
{
    if (!m_refreshThreadDone) {
        m_refreshEnabled = false;
        m_refreshThreadDone = true;
        m_refreshCV.notify_one();
        m_refreshThread.join();
    }
}

void WalletImpl::pauseRefresh()
{
    LOG_PRINT_L2(__FUNCTION__ << ": refresh paused...");
    // TODO synchronize access
    if (!m_refreshThreadDone) {
        m_refreshEnabled = false;
    }
}


bool WalletImpl::isNewWallet() const
{
    // in case wallet created without daemon connection, closed and opened again,
    // it's the same case as if it created from scratch, i.e. we need "fast sync"
    // with the daemon (pull hashes instead of pull blocks).
    // If wallet cache is rebuilt, creation height stored in .keys is used.
    // Watch only wallet is a copy of an existing wallet. 
    return !(blockChainHeight() > 1 || m_recoveringFromSeed || m_recoveringFromDevice || m_rebuildWalletCache) && !watchOnly();
}

void WalletImpl::pendingTxPostProcess(PendingTransactionImpl * pending)
{
  // If the device being used is HW device with cold signing protocol, cold sign then.
  if (!m_wallet->get_account().get_device().has_tx_cold_sign()){
    return;
  }

  tools::wallet2::signed_tx_set exported_txs;
  std::vector<cryptonote::address_parse_info> dsts_info;

  m_wallet->cold_sign_tx(pending->m_pending_tx, exported_txs, dsts_info, pending->m_tx_device_aux);
  pending->m_key_images = exported_txs.key_images;
  pending->m_pending_tx = exported_txs.ptx;
}

bool WalletImpl::doInit(const string &daemon_address, const std::string &proxy_address, uint64_t upper_transaction_size_limit, bool ssl)
{
    if (!m_wallet->init(daemon_address, m_daemon_login, proxy_address, upper_transaction_size_limit))
       return false;

    // in case new wallet, this will force fast-refresh (pulling hashes instead of blocks)
    // If daemon isn't synced a calculated block height will be used instead
    if (isNewWallet() && daemonSynced()) {
        LOG_PRINT_L2(__FUNCTION__ << ":New Wallet - fast refresh until " << daemonBlockChainHeight());
        setRefreshFromBlockHeight(daemonBlockChainHeight());
    }

    if (m_rebuildWalletCache)
      LOG_PRINT_L2(__FUNCTION__ << ": Rebuilding wallet cache, fast refresh until block " << getRefreshFromBlockHeight());

    if (Utils::isAddressLocal(daemon_address)) {
        this->setTrustedDaemon(true);
        m_refreshIntervalMillis = DEFAULT_REFRESH_INTERVAL_MILLIS;
    } else {
        this->setTrustedDaemon(false);
        m_refreshIntervalMillis = DEFAULT_REMOTE_NODE_REFRESH_INTERVAL_MILLIS;
    }
    return true;
}

bool WalletImpl::checkBackgroundSync(const std::string &message) const
{
    clearStatus();
    if (m_wallet->is_background_wallet())
    {
        LOG_ERROR("Background wallets " + message);
        setStatusError(tr("Background wallets ") + message);
        return true;
    }
    if (m_wallet->is_background_syncing())
    {
        LOG_ERROR(message + " while background syncing");
        setStatusError(message + tr(" while background syncing. Stop background syncing first."));
        return true;
    }
    return false;
}

bool WalletImpl::parse_uri(const std::string &uri, std::string &address, std::string &payment_id, uint64_t &amount, std::string &tx_description, std::string &recipient_name, std::vector<std::string> &unknown_parameters, std::string &error)
{
    return m_wallet->parse_uri(uri, address, payment_id, amount, tx_description, recipient_name, unknown_parameters, error);
}

std::string WalletImpl::make_uri(const std::string &address, const std::string &payment_id, uint64_t amount, const std::string &tx_description, const std::string &recipient_name, std::string &error) const
{
    return m_wallet->make_uri(address, payment_id, amount, tx_description, recipient_name, error);
}

std::string WalletImpl::getDefaultDataDir() const
{
 return tools::get_default_data_dir();
}

bool WalletImpl::rescanSpent()
{
  clearStatus();
  if (checkBackgroundSync("cannot rescan spent"))
    return false;
  if (!trustedDaemon()) {
    setStatusError(tr("Rescan spent can only be used with a trusted daemon"));
    return false;
  }
  try {
      m_wallet->rescan_spent();
  } catch (const std::exception &e) {
      LOG_ERROR(__FUNCTION__ << " error: " << e.what());
      setStatusError(e.what());
      return false;
  }
  return true;
}

void WalletImpl::setOffline(bool offline)
{
    m_wallet->set_offline(offline);
}

bool WalletImpl::isOffline() const
{
    return m_wallet->is_offline();
}

void WalletImpl::hardForkInfo(uint8_t &version, uint64_t &earliest_height) const
{
    m_wallet->get_hard_fork_info(version, earliest_height);
}

bool WalletImpl::useForkRules(uint8_t version, int64_t early_blocks) const 
{
    return m_wallet->use_fork_rules(version,early_blocks);
}

bool WalletImpl::blackballOutputs(const std::vector<std::string> &outputs, bool add)
{
    std::vector<std::pair<uint64_t, uint64_t>> raw_outputs;
    raw_outputs.reserve(outputs.size());
    uint64_t amount = std::numeric_limits<uint64_t>::max(), offset, num_offsets;
    for (const std::string &str: outputs)
    {
        if (sscanf(str.c_str(), "@%" PRIu64, &amount) == 1)
          continue;
        if (amount == std::numeric_limits<uint64_t>::max())
        {
          setStatusError("First line is not an amount");
          return true;
        }
        if (sscanf(str.c_str(), "%" PRIu64 "*%" PRIu64, &offset, &num_offsets) == 2 && num_offsets <= std::numeric_limits<uint64_t>::max() - offset)
        {
          while (num_offsets--)
            raw_outputs.push_back(std::make_pair(amount, offset++));
        }
        else if (sscanf(str.c_str(), "%" PRIu64, &offset) == 1)
        {
          raw_outputs.push_back(std::make_pair(amount, offset));
        }
        else
        {
          setStatusError(tr("Invalid output: ") + str);
          return false;
        }
    }
    bool ret = m_wallet->set_blackballed_outputs(raw_outputs, add);
    if (!ret)
    {
        setStatusError(tr("Failed to mark outputs as spent"));
        return false;
    }
    return true;
}

bool WalletImpl::blackballOutput(const std::string &amount, const std::string &offset)
{
    uint64_t raw_amount, raw_offset;
    if (!epee::string_tools::get_xtype_from_string(raw_amount, amount))
    {
        setStatusError(tr("Failed to parse output amount"));
        return false;
    }
    if (!epee::string_tools::get_xtype_from_string(raw_offset, offset))
    {
        setStatusError(tr("Failed to parse output offset"));
        return false;
    }
    bool ret = m_wallet->blackball_output(std::make_pair(raw_amount, raw_offset));
    if (!ret)
    {
        setStatusError(tr("Failed to mark output as spent"));
        return false;
    }
    return true;
}

bool WalletImpl::unblackballOutput(const std::string &amount, const std::string &offset)
{
    uint64_t raw_amount, raw_offset;
    if (!epee::string_tools::get_xtype_from_string(raw_amount, amount))
    {
        setStatusError(tr("Failed to parse output amount"));
        return false;
    }
    if (!epee::string_tools::get_xtype_from_string(raw_offset, offset))
    {
        setStatusError(tr("Failed to parse output offset"));
        return false;
    }
    bool ret = m_wallet->unblackball_output(std::make_pair(raw_amount, raw_offset));
    if (!ret)
    {
        setStatusError(tr("Failed to mark output as unspent"));
        return false;
    }
    return true;
}

bool WalletImpl::getRing(const std::string &key_image, std::vector<uint64_t> &ring) const
{
    crypto::key_image raw_key_image;
    if (!epee::string_tools::hex_to_pod(key_image, raw_key_image))
    {
        setStatusError(tr("Failed to parse key image"));
        return false;
    }
    bool ret = m_wallet->get_ring(raw_key_image, ring);
    if (!ret)
    {
        setStatusError(tr("Failed to get ring"));
        return false;
    }
    return true;
}

bool WalletImpl::getRings(const std::string &txid, std::vector<std::pair<std::string, std::vector<uint64_t>>> &rings) const
{
    crypto::hash raw_txid;
    if (!epee::string_tools::hex_to_pod(txid, raw_txid))
    {
        setStatusError(tr("Failed to parse txid"));
        return false;
    }
    std::vector<std::pair<crypto::key_image, std::vector<uint64_t>>> raw_rings;
    bool ret = m_wallet->get_rings(raw_txid, raw_rings);
    if (!ret)
    {
        setStatusError(tr("Failed to get rings"));
        return false;
    }
    for (const auto &r: raw_rings)
    {
      rings.push_back(std::make_pair(epee::string_tools::pod_to_hex(r.first), r.second));
    }
    return true;
}

bool WalletImpl::setRing(const std::string &key_image, const std::vector<uint64_t> &ring, bool relative)
{
    crypto::key_image raw_key_image;
    if (!epee::string_tools::hex_to_pod(key_image, raw_key_image))
    {
        setStatusError(tr("Failed to parse key image"));
        return false;
    }
    bool ret = m_wallet->set_ring(raw_key_image, ring, relative);
    if (!ret)
    {
        setStatusError(tr("Failed to set ring"));
        return false;
    }
    return true;
}

void WalletImpl::segregatePreForkOutputs(bool segregate)
{
    m_wallet->segregate_pre_fork_outputs(segregate);
}

void WalletImpl::segregationHeight(uint64_t height)
{
    m_wallet->segregation_height(height);
}

void WalletImpl::keyReuseMitigation2(bool mitigation)
{
    m_wallet->key_reuse_mitigation2(mitigation);
}

bool WalletImpl::lockKeysFile()
{
    return m_wallet->lock_keys_file();
}

bool WalletImpl::unlockKeysFile()
{
    return m_wallet->unlock_keys_file();
}

bool WalletImpl::isKeysFileLocked()
{
    return m_wallet->is_keys_file_locked();
}

uint64_t WalletImpl::coldKeyImageSync(uint64_t &spent, uint64_t &unspent)
{
    return m_wallet->cold_key_image_sync(spent, unspent);
}

void WalletImpl::deviceShowAddress(uint32_t accountIndex, uint32_t addressIndex, const std::string &paymentId)
{
    boost::optional<crypto::hash8> payment_id_param = boost::none;
    if (!paymentId.empty())
    {
        crypto::hash8 payment_id;
        bool res = tools::wallet2::parse_short_payment_id(paymentId, payment_id);
        if (!res)
        {
            throw runtime_error("Invalid payment ID");
        }
        payment_id_param = payment_id;
    }

    m_wallet->device_show_address(accountIndex, addressIndex, payment_id_param);
}

bool WalletImpl::reconnectDevice()
{
    clearStatus();

    bool r;
    try {
        r = m_wallet->reconnect_device();
    }
    catch (const std::exception &e) {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
        return false;
    }

    return r;
}

uint64_t WalletImpl::getBytesReceived()
{
    return m_wallet->get_bytes_received();
}

uint64_t WalletImpl::getBytesSent()
{
    return m_wallet->get_bytes_sent();
}

//-------------------------------------------------------------------------------------------------------------------
std::string WalletImpl::getMultisigSeed(const std::string &seed_offset)
{
    clearStatus();

    if (!multisig().isMultisig)
    {
      setStatusError(tr("Wallet is not multisig"));
      return "";
    }

    epee::wipeable_string seed;
    try
    {
        if (m_wallet->get_multisig_seed(seed, seed_offset))
            return std::string(seed.data(), seed.size());
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(string(tr("Failed to get multisig seed: ")) + e.what());
    }
    return "";
}
//-------------------------------------------------------------------------------------------------------------------
std::pair<std::uint32_t, std::uint32_t> WalletImpl::getSubaddressIndex(const std::string &address)
{
    clearStatus();

    cryptonote::address_parse_info info;
    std::pair<std::uint32_t, std::uint32_t> indices{0, 0};
    if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), address))
    {
        setStatusError(tr("Failed to parse address"));
        return indices;
    }

    auto index = m_wallet->get_subaddress_index(info.address);
    if (!index)
        setStatusError(tr("Address doesn't belong to the wallet"));
    else
        indices = std::make_pair(index.major, index.minor);

    return indices;
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::freeze(std::size_t idx)
{
    clearStatus();

    try
    {
        m_wallet->freeze(idx);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::freeze(const std::string &key_image)
{
    try
    {
        freeze(getTransferIndex(key_image));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::thaw(std::size_t idx)
{
    clearStatus();

    try
    {
        m_wallet->thaw(idx);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::thaw(const std::string &key_image)
{
    try
    {
        thaw(getTransferIndex(key_image));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::isFrozen(std::size_t idx)
{
    clearStatus();

    try
    {
        return m_wallet->frozen(idx);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::isFrozen(const std::string &key_image)
{
    try
    {
        return isFrozen(getTransferIndex(key_image));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool isFrozen(const PendingTransaction &multisig_ptxs)
{
    clearStatus();

    try
    {
        checkMultisigWalletReady(m_wallet);

        tools::wallet2::multisig_tx_set multisig_tx_set;
        multisig_tx_set.m_ptx = multisig_ptxs->m_pending_tx;
        multisig_tx_set.m_signers = multisig_ptxs->m_signers;

        return m_wallet->frozen(multisig_tx_set);
    }
    catch (const exception &e)
    {
        setStatusError(e.what());
    }

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool isFrozen(const std::string multisig_sign_data)
{
    clearStatus();

    try
    {
        checkMultisigWalletReady(m_wallet);

        string multisig_sign_data_bin;
        if (!epee::string_tools::parse_hexstr_to_binbuff(multisig_sign_data, multisig_sign_data_bin))
            throw runtime_error(tr("Failed to deserialize multisig transaction"));

        tools::wallet2::multisig_tx_set multisig_txs;
        if (!m_wallet->load_multisig_tx(multisig_sign_data_bin, multisig_txs, {}))
            throw runtime_error(tr("Failed to load multisig transaction"));

        return m_wallet->frozen(multisig_txs);
    }
    catch (const exception &e)
    {
        setStatusError(e.what());
    }

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::createOneOffSubaddress(std::uint32_t account_index, std::uint32_t address_index)
{
    m_wallet->create_one_off_subaddress({account_index, address_index});
    // TODO : Figure out if we need to call one of those after creating the one off subaddress.
//    m_subaddress->refresh(accountIndex);
//    m_subaddressAccount->refresh();
}
//-------------------------------------------------------------------------------------------------------------------
WalletState WalletImpl::getWalletState()
{
    WalletState wallet_state{};
    wallet_state.is_deprecated = m_wallet->is_deprecated();

    return wallet_state;
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::hasUnknownKeyImages()
{
    return m_wallet->has_unknown_key_images();
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::rewrite(const std::string &wallet_name, const std::string &password)
{
    clearStatus();

    try
    {
        m_wallet->rewrite(wallet_name, epee::wipeable_string(password.data(), password.size()));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::writeWatchOnlyWallet(const std::string &wallet_name, const std::string &password, std::string &new_keys_file_name)
{
    clearStatus();

    try
    {
        m_wallet->write_watch_only_wallet(wallet_name, epee::wipeable_string(password.data(), password.size()), new_keys_file_name);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
std::map<std::uint32_t, std::uint64_t> WalletImpl::balancePerSubaddress(std::uint32_t index_major, bool strict)
{
    return m_wallet->balance_per_subaddress(index_major, strict);
}
//-------------------------------------------------------------------------------------------------------------------
std::map<std::uint32_t, std::pair<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>>> WalletImpl::unlockedBalancePerSubaddress(std::uint32_t index_major, bool strict)
{
    return m_wallet->unlocked_balance_per_subaddress(index_major, strict);
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::isTransferUnlocked(std::uint64_t unlock_time, std::uint64_t block_height)
{
    return m_wallet->is_transfer_unlocked(unlock_time, block_height);
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::updatePoolState(std::vector<std::tuple<cryptonote::transaction, std::string, bool>> &process_txs, bool refreshed, bool try_incremental)
{
    clearStatus();

    std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> process_txs_pod;
    try
    {
        m_wallet->update_pool_state(process_txs_pod, refreshed, try_incremental);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
        return;
    }

    std::string tx_id;
    process_txs.reserve(process_txs_pod.size());
    for (auto &tx : process_txs_pod)
    {
        if (!epee::string_tools::pod_to_hex(std::get<1>(tx), tx_id))
        {
            setStatusError(tr("Failed to parse tx_id"));
            return;
        }
        process_txs.push_back(std::make_tuple(std::get<0>(tx), tx_id, std::get<2>(tx)));
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::processPoolState(const std::vector<std::tuple<cryptonote::transaction, std::string, bool>> &txs)
{
    clearStatus();

    std::vector<std::tuple<cryptonote::transaction, crypto::hash, bool>> txs_pod;
    crypto::hash tx_id;
    for (auto &tx : txs)
    {
        if (!epee::string_tools::hex_to_pod(std::get<1>(tx), tx_id))
        {
            setStatusError(tr("Failed to parse tx_id"));
            return;
        }
        txs_pod.push_back(std::make_tuple(std::get<0>(tx), tx_id, std::get<2>(tx)));
    }

    try
    {
        m_wallet->process_pool_state(txs_pod);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
std::string convertMultisigTxToStr(const PendingTransaction &multisig_ptx)
{
    clearStatus();

    try
    {
        checkMultisigWalletReady(m_wallet);

        tools::wallet2::multisig_tx_set multisig_tx_set;
        multisig_tx_set.m_ptx = multisig_ptxs.m_pending_tx;
        multisig_tx_set.m_signers = multisig_ptxs.m_signers;

        return m_wallet->save_multisig_tx(multisig_tx_set);
    }
    catch (const exception &e)
    {
        setStatusError(string(tr("Failed to convert pending multisig tx to string: ")) + e.what());
    }

    return "";
}
//-------------------------------------------------------------------------------------------------------------------
bool saveMultisigTx(const PendingTransaction &multisig_ptxs, const std::string &filename)
{
    clearStatus();

    try
    {
        checkMultisigWalletReady(m_wallet);

        tools::wallet2::multisig_tx_set multisig_tx_set;
        multisig_tx_set.m_ptx = multisig_ptxs.m_pending_tx;
        multisig_tx_set.m_signers = multisig_ptxs.m_signers;

        return m_wallet->save_multisig_tx(multisig_tx_set, filename);
    }
    catch (const exception &e)
    {
        setStatusError(e.what());
    }

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
std::string convertTxToStr(const PendingTransaction &ptxs)
{
    clearStatus();

    std::string tx_dump = m_wallet->dump_tx_to_str(ptxs.m_pending_tx);
    if (tx_dump.empty())
    {
        setStatusError("Failed to convert pending tx to string");
    }

    return tx_dump;
}
//-------------------------------------------------------------------------------------------------------------------
bool parseUnsignedTxFromStr(const std::string &unsigned_tx_str, UnsignedTransaction &exported_txs)
{
    return m_wallet->parse_unsigned_tx_from_str(unsigned_tx_str, exported_txs.m_unsigned_tx_set);
}
//-------------------------------------------------------------------------------------------------------------------
bool parseMultisigTxFromStr(const std::string &multisig_tx_str, PendingTransaction &exported_txs)
{
    clearStatus();

    try
    {
        checkMultisigWalletReady(m_wallet);

        tools::wallet2::multisig_tx_set multisig_tx;

        if (!m_wallet->parse_multisig_tx_from_str(multisig_tx_str, multisig_tx))
            throw runtime_error(tr("Failed to parse multisig transaction from string."));

        exported_txs.m_pending_tx = multisig_tx.m_ptx;
        exported_txs.m_signers = multisig_tx.m_signers;

        return true;
    }
    catch (const exception &e)
    {
        setStatusError(e.what());
    }

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
//bool loadMultisigTxFromFile(const std::string &filename, PendingTransaction &exported_txs, std::function<bool(const PendingTransaction&)> accept_func)
//{
//    clearStatus();
//
//    try
//    {
//        checkMultisigWalletReady(m_wallet);
//
//        tools::wallet2::multisig_tx_set multisig_tx;
//
//        // QUESTION / TODO : How to translate the accept_func?
//        if (!m_wallet->load_multisig_tx_from_file(filename, multisig_tx, accept_func))
//            throw runtime_error(tr("Failed to load multisig transaction from file."));
//
//        exported_txs.m_pending_tx = multisig_tx.m_ptx;
//        exported_txs.m_signers = multisig_tx.m_signers;
//
//        return true;
//    }
//    catch (const exception &e)
//    {
//        setStatusError(e.what());
//    }
//
//    return false;
//}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t WalletImpl::getFeeMultiplier(std::uint32_t priority, int fee_algorithm)
{
    return m_wallet->get_fee_multiplier(priority, fee_algorithm);
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t WalletImpl::getBaseFee()
{
    bool use_dyn_fee = useForkRules(HF_VERSION_DYNAMIC_FEE, -30 * 1);
    if (!use_dyn_fee)
        return FEE_PER_KB;

    return m_wallet->get_dynamic_base_fee_estimate();
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t WalletImpl::getMinRingSize()
{
    if (useForkRules(HF_VERSION_MIN_MIXIN_15, 0))
        return 16;
    if (useForkRules(8, 10))
        return 11;
    if (useForkRules(7, 10))
        return 7;
    if (useForkRules(6, 10))
        return 5;
    if (useForkRules(2, 10))
        return 3;
    return 0;
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t WalletImpl::adjustMixin(std::uint64_t mixin)
{
    return m_wallet->adjust_mixin(mixin);
}
//-------------------------------------------------------------------------------------------------------------------
std::uint32_t WalletImpl::adjustPriority(std::uint32_t priority)
{
    clearStatus();

    try
    {
        return m_wallet->adjust_priority(priority);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(__FUNCTION__ << " error: " << e.what());
        setStatusError(string(tr("Failed to adjust priority: ")) + e.what());
    }
    return 0;
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::unsetRing(const std::vector<std::string> &key_images)
{
    clearStatus();

    crypto::key_image ki_pod;
    std::vector<crypto::key_image> key_images_pod;
    for (std::string &ki : key_images)
    {
        if (!epee::string_tools::hex_to_pod(ki, ki_pod))
        {
            // QUESTION : I think it may be a good idea to add the string that failed to parse to the error message as done below.
            //            Do you agree? Then I'll add it to other "Failed to parse ..." messages above, else I'll remove it from messages below.
            setStatusError((boost::format(tr("Failed to parse key image: %s")) % ki).str());
            return false;
        }
        key_images_pod.push_back(ki_pod);
    }
    return m_wallet->unset_ring(key_images_pod);
    // TODO : figure out how the API could handle m_ringdb and m_ringdb_key itself, to get rid of wallet2 for this function
    //        and we can improve error feedback, currently `m_wallet->unset_ring()` consumes the exception thrown by `m_ringdb->remove_rings()`
//    try
//    {
//        return m_ringdb->remove_rings(getRingdbKey(), key_images_pod);
//    }
//    catch (const std::exception &e)
//    {
//        setStatusError(e.what());
//    }
//    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::unsetRing(const std::string &tx_id)
{
    clearStatus();

    crypto::hash tx_id_pod;
    if (!epee::string_tools::hex_to_pod(tx_id, tx_id_pod))
    {
        setStatusError((boost::format(tr("Failed to parse tx_id: %s")) % tx_id).str());
        return false;
    }

    try
    {
        return m_wallet->unset_ring(tx_id_pod);
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
    }
    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::findAndSaveRings(bool force)
{
    clearStatus();

    try
    {
        return m_wallet->find_and_save_rings(force);
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
    }
    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::isOutputBlackballed(const std::pair<std::uint64_t, std::uint64_t> &output)
{
    return m_wallet->is_output_blackballed(output);
}
//-------------------------------------------------------------------------------------------------------------------
void coldTxAuxImport(const PendingTransaction &ptx, const std::vector<std::string> &tx_device_aux)
{
    clearStatus();

    try
    {
        m_wallet->cold_tx_aux_import(ptx->m_pending_tx, tx_device_aux);
    }
    catch (const std::exception &e)
    {
        setStatusError(e.what());
    }
}
//-------------------------------------------------------------------------------------------------------------------
// TODO : Somehow missed that we already had this in the API. Imo this should replace the old one because it handles error.
//bool WalletImpl::useForkRules(std::uint8_t version, std::int64_t early_blocks)
//{
//    clearStatus();
//
//    try
//    {
//        return m_wallet->use_fork_rules(version, early_blocks);
//    }
//    catch (const std::exception &e)
//    {
//        setStatusError((boost::format(tr("Failed to check if we use fork rules for version `%u` with `%d` early blocks. Error: %s")) % version % early_blocks % e.what()).str());
//    }
//    return false;
//}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::discardUnmixableOutputs()
{
    clearStatus();

    try
    {
        m_wallet->discard_unmixable_outputs();
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to discard unmixable outputs. Error: %s")) % e.what()).str());
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::setTxKey(const std::string &txid, const std::string &tx_key, const std::vector<std::string> &additional_tx_keys, const boost::optional<std::string> &single_destination_subaddress)
{
    clearStatus();

    crypto::hash txid_pod;
    if (!epee::string_tools::hex_to_pod(txid, txid_pod))
    {
        setStatusError((boost::format(tr("Failed to parse tx_id: %s")) % tx_id).str());
        return;
    }

    crypto::secret_key tx_key_pod;
    if (!epee::string_tools::hex_to_pod(tx_key, tx_key_pod))
    {
        setStatusError((boost::format(tr("Failed to parse tx_key: %s")) % tx_key).str());
        return;
    }

    std::vector<crypto::secret_key> additional_tx_keys_pod;
    crypto::secret_key tmp_additional_tx_key_pod;
    additional_tx_key_pod.reserve(additional_tx_keys.size());
    for (std::string additional_tx_key : additional_tx_keys)
    {
        if (!epee::string_tools::hex_to_pod(additional_tx_key, tmp_additional_tx_key_pod))
        {
            setStatusError((boost::format(tr("Failed to parse additional_tx_key: %s")) % additional_tx_key).str());
            return;
        }
        additional_tx_keys_pod.push_back(tmp_additional_tx_key_pod);
    }

    boost::optional<cryptonote::account_public_address> single_destination_subaddress_pod = boost::none;
    cryptonote::address_parse_info info;
    if (single_destination_subaddress != boost::none)
    {
        if (!cryptonote::get_account_address_from_str(info, m_wallet->nettype(), single_destination_subaddress))
        {
            setStatusError((boost::format(tr("Failed to parse subaddress: %s")) % single_destination_subaddress).str());
            return;
        }
        single_destination_subaddress_pod = info.address;
    }

    try
    {
        m_wallet->set_tx_key(txid_pod, tx_key_pod, additional_tx_keys_pod, info.address);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to set tx key. Error: %s")) % e.what()).str());
    }
}
//-------------------------------------------------------------------------------------------------------------------
std::string WalletImpl::getDaemonAddress()
{
    return m_wallet->get_daemon_address();
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t WalletImpl::getDaemonAdjustedTime()
{
    clearStatus();

    try
    {
        return m_wallet->get_daemon_adjusted_time();
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to get daemon adjusted time. Error: %s")) % e.what()).str());
    }
    return 0;
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::setCacheDescription(const std::string &description)
{
    m_wallet->set_description(description);
    // TODO : Add ATTRIBUTE_DESCRIPTION to API and then call
//    setCacheAttribute(ATTRIBUTE_DESCRIPTION, description);
}
//-------------------------------------------------------------------------------------------------------------------
std::string WalletImpl::getCacheDescription()
{
    return m_wallet->get_description();
    // TODO : Add ATTRIBUTE_DESCRIPTION to API and then call
//    return getCacheAttribute(ATTRIBUTE_DESCRIPTION);
}
//-------------------------------------------------------------------------------------------------------------------
const std::pair<std::map<std::string, std::string>, std::vector<std::string>>& WalletImpl::getAccountTags()
{
    return m_wallet->get_account_tags();
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::setAccountTag(const std::set<uint32_t> &account_indices, const std::string &tag)
{
    clearStatus();

    try
    {
        m_wallet->set_account_tag(account_indices, tag);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to set account tag. Error: %s")) % e.what()).str());
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::setAccountTagDescription(const std::string &tag, const std::string &description)
{
    clearStatus();

    try
    {
        m_wallet->set_account_tag_description(tag, description);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to set account tag description. Error: %s")) % e.what()).str());
    }
}
//-------------------------------------------------------------------------------------------------------------------
std::string WalletImpl::exportOutputsToStr(bool all, std::uint32_t start, std::uint32_t count)
{
    clearStatus();

    try
    {
        return m_wallet->export_outputs_to_str(all, start, count);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to export outputs to string. Error: %s")) % e.what()).str());
    }
    return "";
}
//-------------------------------------------------------------------------------------------------------------------
std::size_t WalletImpl::importOutputsFromStr(const std::string &outputs_str)
{
    clearStatus();

    try
    {
        return m_wallet->import_outputs_from_str(outputs_str);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to import outputs from string. Error: %s")) % e.what()).str());
    }
    return 0;
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t WalletImpl::getBlockchainHeightByDate(std::uint16_t year, std::uint8_t month, std::uint8_t day)
{
    clearStatus();

    try
    {
        return m_wallet->get_blockchain_height_by_date(year, month, day);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to get blockchain height by date. Error: %s")) % e.what()).str());
    }
    return 0;
}
//-------------------------------------------------------------------------------------------------------------------
bool isSynced()
{
    clearStatus();

    try
    {
        return m_wallet->is_synced();
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to check if wallet is synced. Error: %s")) % e.what()).str());
    }
    return 0;
}
//-------------------------------------------------------------------------------------------------------------------
std::vector<std::pair<std::uint64_t, std::uint64_t>> WalletImpl::estimateBacklog(const std::vector<std::pair<double, double>> &fee_levels)
{
    clearStatus();

    try
    {
        return m_wallet->estimate_backlog(fee_levels);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to estimate backlog. Error: %s")) % e.what()).str());
    }
    return { std::make_pair(0, 0) };
}
//-------------------------------------------------------------------------------------------------------------------
std::vector<std::pair<std::uint64_t, std::uint64_t>> WalletImpl::estimateBacklog(std::uint64_t min_tx_weight, std::uint64_t max_tx_weight, const std::vector<std::uint64_t> &fees)
{
    clearStatus();

    if (min_tx_weight == 0 || max_tx_weight == 0)
    {
        setStatusError("Invalid 0 weight");
        return { std::make_pair(0, 0) };
    }
    for (std::uint64_t fee: fees)
    {
        if (fee == 0)
        {
            setStatusError("Invalid 0 fee");
            return { std::make_pair(0, 0) };
        }
    }

    std::vector<std::pair<double, double>> fee_levels;
    for (uint64_t fee: fees)
    {
        double our_fee_byte_min = fee / (double)min_tx_weight, our_fee_byte_max = fee / (double)max_tx_weight;
        fee_levels.emplace_back(our_fee_byte_min, our_fee_byte_max);
    }
    return estimateBacklog(fee_levels);
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::saveToFile(const std::string &path_to_file, const std::string &binary, bool is_printable = false)
{
    return m_wallet->save_to_file(path_to_file, binary, is_printable);
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletImpl::loadFromFile(const std::string &path_to_file, std::string &target_str, std::size_t max_size = 1000000000)
{
    return m_wallet->load_from_file(path_to_file, target_str, max_size);
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t WalletImpl::hashTransfers(boost::optional<std::uint64_t> transfer_height, std::string &hash)
{
    clearStatus();

    try
    {
        return m_wallet->hash_m_transfers(transfer_height, hash);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to hash transfers. Error: %s")) % e.what()).str());
    }
    return 0;
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::finishRescanBcKeepKeyImages(std::uint64_t transfer_height, const std::string &hash)
{
    clearStatus();

    try
    {
        m_wallet->finish_rescan_bc_keep_key_images(transfer_height, hash);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to finish rescan blockchain. Error: %s")) % e.what()).str());
    }
}
//-------------------------------------------------------------------------------------------------------------------
std::pair<std::size_t, std::uint64_t> WalletImpl::estimateTxSizeAndWeight(bool use_rct, int n_inputs, int ring_size, int n_outputs, std::size_t extra_size)
{
    clearStatus();

    try
    {
        return m_wallet->estimate_tx_size_and_weight(use_rct, n_inputs, ring_size, n_outputs, extra_size);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to estimate transaction size and weight. Error: %s")) % e.what()).str());
    }
    return std::make_pair(0, 0);
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t importKeyImages(const std::vector<std::pair<std::string, std::string>> &signed_key_images, size_t offset, std::uint64_t &spent, std::uint64_t &unspent, bool check_spent)
{
    clearStatus();

    std::vector<std::pair<crypto::key_image, crypto::signature>> signed_key_images_pod;
    crypto::key_image tmp_key_image_pod;
    crypto::signature tmp_signature_pod{};
    size_t sig_size = sizeof(crypto::signature);

    signed_key_images_pod.reserve(signed_key_images.size());

    for (auto ski : signed_key_images)
    {
        if (!epee::string_tools::hex_to_pod(ski.first, tmp_key_image_pod))
        {
            setStatusError((boost::format(tr("Failed to parse key_image: %s")) % ski.first).str());
            return false;
        }
        if (!epee::string_tools::hex_to_pod(ski.second.substr(0, sig_size/2), tmp_signature.c))
        {
            setStatusError((boost::format(tr("Failed to parse signature.c: %s")) % ski.second.substr(0, sig_size/2)).str());
            return false;
        }
        if (!epee::string_tools::hex_to_pod(ski.second.substr(sig_size/2, sig_size), tmp_signature.r))
        {
            setStatusError((boost::format(tr("Failed to parse signature.r: %s")) % ski.substr(sig_size/2, sig_size)).str());
            return false;
        }
        signed_key_images_pod.push_back(std::make_pair(tmp_key_image_pod, tmp_signature));
    }

    try
    {
        return m_wallet->import_key_images(signed_key_images_pod, offset, spent, unspent, check_spent);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to import key images. Error: %s")) % e.what()).str());
    }
    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool importKeyImages(std::vector<std::string> key_images, size_t offset, boost::optional<std::unordered_set<size_t>> selected_transfers = boost::none)
{
    clearStatus();

    std::vector<crypto::key_image> key_images_pod;
    crypto::key_image tmp_key_image_pod;
    key_images_pod.reserve(key_images.size());
    for (std::string key_image : key_images)
    {
        if (!epee::string_tools::hex_to_pod(key_image, tmp_key_image_pod))
        {
            setStatusError((boost::format(tr("Failed to parse key_image: %s")) % key_image).str());
            return false;
        }
        key_images_pod.push_back(tmp_key_image_pod);
    }

    try
    {
        return m_wallet->import_key_images(key_images_pod, offset, selected_transfers);
    }
    catch (const std::exception &e)
    {
        setStatusError((boost::format(tr("Failed to import key images. Error: %s")) % e.what()).str());
    }
    return false;
}
//-------------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------------
// PRIVATE
//-------------------------------------------------------------------------------------------------------------------
std::size_t WalletImpl::getTransferIndex(const std::string &key_image)
{
    crypto::key_image ki;
    if (!epee::string_tools::hex_to_pod(key_image, ki))
        throw runtime_error(tr("Failed to parse key image."));

    return m_wallet->get_transfer_details(ki);
}
//-------------------------------------------------------------------------------------------------------------------
void WalletImpl::makeMultisigTxSet(PendingTransaction &ptx)
{
    if (!ptx->m_wallet->multisig().isMultisig)
        throw runtime_error(tr("Wallet is not multisig"));

    auto multisig_tx = m_wallet->make_multisig_tx_set(ptx.m_pending_tx);
    ptx.m_signers = multisig_tx.m_signers;
}
//-------------------------------------------------------------------------------------------------------------------

} // namespace
