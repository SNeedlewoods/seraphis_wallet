// Copyright (c) 2014-2023, The Monero Project
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

#ifndef WALLET_IMPL_H
#define WALLET_IMPL_H

#include "wallet/api/wallet2_api.h"
#include "wallet/wallet2.h"

#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp>

class WalletApiAccessorTest;

namespace Monero {
class TransactionHistoryImpl;
class PendingTransactionImpl;
class UnsignedTransactionImpl;
class AddressBookImpl;
class SubaddressImpl;
class SubaddressAccountImpl;
struct Wallet2CallbackImpl;

class WalletImpl : public Wallet
{
public:
    WalletImpl(NetworkType nettype = MAINNET, uint64_t kdf_rounds = 1);
    ~WalletImpl();
    // TODO : not sure about the following static functions
    //        they're used in simplewallet and wallet_rpc_server
    //        but they don't fit into wallet2_api.h because there we only use standard types for arguments
    //        (or structs that were also declared in wallet2_api.h)
    /**
    * brief: hasTestnetOption -
    * param: vm -
    * return: true if testnet option in vm is set
    */
    static bool hasTestnetOption(const boost::program_options::variables_map &vm) { return tools::wallet2::has_testnet_option(vm); }
    /**
    * brief: hasStagenetOption -
    * param: vm -
    * return: true if stagenet option in vm is set
    */
    static bool hasStagenetOption(const boost::program_options::variables_map &vm) { return tools::wallet2::has_stagenet_option(vm); }
    /**
    * brief: deviceNameOption -
    * param: vm -
    * return: name of device in hw-device option
    */
    static std::string deviceNameOption(const boost::program_options::variables_map &vm) { return tools::wallet2::device_name_option(vm); }
    /**
    * brief: deviceDerivationPathOption -
    * param: vm -
    * return: path set in hw-device-deriv-path option
    */
    static std::string deviceDerivationPathOption(const boost::program_options::variables_map &vm) { return tools::wallet2::device_derivation_path_option(vm); }
    /**
    * brief: initOptions - initialize options for command line arguments
    * param: desc_params -
    */
    static void initOptions(boost::program_options::options_description &desc_params) { tools::wallet2::init_options(desc_params); }
    /**
    * brief: makeFromJson - use json_file to generate a wallet
    * param: vm -
    * param: unattended -
    * param: json_file -
    * param: password_prompter -
    * return: wallet2 and password - if no error
    */
    static std::pair<std::unique_ptr<tools::wallet2>, tools::password_container>
        makeFromJson(const boost::program_options::variables_map &vm,
                     bool unattended,
                     const std::string &json_file,
                     const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
    { return tools::wallet2::make_from_json(vm, unattended, json_file, password_prompter); }
    /**
    * brief: makeFromFile - make a wallet or use wallet_file to load a wallet
    * param: vm -
    * param: unattended -
    * param: wallet_file -
    * param: password_prompter -
    * return: wallet2 and password - if no error
    */
    static std::pair<std::unique_ptr<tools::wallet2>, tools::password_container>
        makeFromFile(const boost::program_options::variables_map &vm,
                     bool unattended,
                     const std::string &wallet_file,
                     const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
    { return tools::wallet2::make_from_file(vm, unattended, wallet_file, password_prompter); }
    /**
    * brief: makeNew - make a new wallet
    * param: vm -
    * param: unattended -
    * param: password_prompter -
    * return: wallet2 and password - if no error
    */
    static std::pair<std::unique_ptr<tools::wallet2>, tools::password_container>
        makeNew(const boost::program_options::variables_map &vm,
                bool unattended,
                const std::function<boost::optional<tools::password_container>(const char *, bool)> &password_prompter)
    { return tools::wallet2::make_new(vm, unattended, password_prompter); }

    // TODO : also not sure about these functions, because they're already available in wallet_manager
    //static bool verify_password(const std::string& keys_file_name, const epee::wipeable_string& password, bool no_spend_key, hw::device &hwdev, uint64_t kdf_rounds);
    //static bool query_device(hw::device::device_type& device_type, const std::string& keys_file_name, const epee::wipeable_string& password, uint64_t kdf_rounds = 1);

    // Overrides
    std::string seed(const std::string &seed_offset = "") const override;
    std::string getSeedLanguage() const override;
    void setSeedLanguage(const std::string &arg) override;
    int status() const override;
    std::string errorString() const override;
    void statusWithErrorString(int &status, std::string &errorString) const override;
    bool setPassword(const std::string &passphrase) override;
    const std::string& getPassword() const override;
    bool setDevicePin(const std::string &password) override;
    bool setDevicePassphrase(const std::string &password) override;
    std::string address(uint32_t accountIndex = 0, uint32_t addressIndex = 0) const override;
    std::string path() const override;
    void hardForkInfo(uint8_t version, uint64_t &earliest_height) const override;
    bool useForkRules(uint8_t version, int64_t early_blocks) const override;
    std::string integratedAddress(const std::string &payment_id) const override;
    std::string secretViewKey() const override;
    std::string publicViewKey() const override;
    std::string secretSpendKey() const override;
    std::string publicSpendKey() const override;
    std::string publicMultisigSignerKey() const override;
    void stop() override;
    bool store(const std::string &path) override;
    std::string filename() const override;
    std::string keysFilename() const override;
    bool init(const std::string &daemon_address, uint64_t upper_transaction_size_limit = 0, const std::string &daemon_username = "", const std::string &daemon_password = "", bool use_ssl = false, bool lightWallet = false, const std::string &proxy_address = "") override;
    bool createWatchOnly(const std::string &path, const std::string &password, const std::string &language) const override;
    void setRefreshFromBlockHeight(uint64_t refresh_from_block_height) override;
    void setRecoveringFromSeed(bool recoveringFromSeed) override;
    void setRecoveringFromDevice(bool recoveringFromDevice) override;
    void setSubaddressLookahead(uint32_t major, uint32_t minor) override;
    bool connectToDaemon() override;
    ConnectionStatus connected() const override;
    void setTrustedDaemon(bool arg) override;
    bool trustedDaemon() const override;
    bool setProxy(const std::string &address) override;
    uint64_t balance(uint32_t accountIndex = 0) const override;
    uint64_t unlockedBalance(uint32_t accountIndex = 0) const override;
    bool watchOnly() const override;
    bool isDeterministic() const override;
    uint64_t blockChainHeight() const override;
    uint64_t approximateBlockChainHeight() const override;
    uint64_t estimateBlockChainHeight() const override;
    uint64_t daemonBlockChainHeight() const override;
    uint64_t daemonBlockChainTargetHeight() const override;
    bool synchronized() const override;
    void startRefresh() override;
    void pauseRefresh() override;
    bool refresh() override;
    void refreshAsync() override;
    bool rescanBlockchain() override;
    void rescanBlockchainAsync() override;
    void setAutoRefreshInterval(int millis) override;
    int autoRefreshInterval() const override;
    void addSubaddressAccount(const std::string &label) override;
    size_t numSubaddressAccounts() const override;
    size_t numSubaddresses(uint32_t accountIndex) const override;
    void addSubaddress(uint32_t accountIndex, const std::string &label) override;
    std::string getSubaddressLabel(uint32_t accountIndex, uint32_t addressIndex) const override;
    void setSubaddressLabel(uint32_t accountIndex, uint32_t addressIndex, const std::string &label) override;
    PendingTransaction * createTransactionMultDest(const std::vector<std::string> &dst_addr, const std::string &payment_id, optional<std::vector<uint64_t>> amount, uint32_t mixin_count, PendingTransaction::Priority priority = PendingTransaction::Priority_Low, uint32_t subaddr_account = 0, std::set<uint32_t> subaddr_indices = {}) override;
    PendingTransaction * createTransaction(const std::string &dst_addr, const std::string &payment_id, optional<uint64_t> amount, uint32_t mixin_count, PendingTransaction::Priority priority = PendingTransaction::Priority_Low, uint32_t subaddr_account = 0, std::set<uint32_t> subaddr_indices = {}) override;
    PendingTransaction * createSweepUnmixableTransaction() override;
    UnsignedTransaction * loadUnsignedTx(const std::string &unsigned_filename) override;
    bool submitTransaction(const std::string &fileName) override;
    void disposeTransaction(PendingTransaction *t) override;
    uint64_t estimateTransactionFee(const std::vector<std::pair<std::string, uint64_t>> &destinations, PendingTransaction::Priority priority) const override;
    bool exportKeyImages(const std::string &filename, bool all = false) override;
    bool importKeyImages(const std::string &filename) override;
    bool exportOutputs(const std::string &filename, bool all = false) override;
    bool importOutputs(const std::string &filename) override;
    bool scanTransactions(const std::vector<std::string> &txids) override;
    TransactionHistory * history() override;
    AddressBook * addressBook() override;
    Subaddress * subaddress() override;
    SubaddressAccount * subaddressAccount() override;
    void setListener(WalletListener *l) override;
    uint32_t defaultMixin() const override;
    void setDefaultMixin(uint32_t arg) override;
    bool setCacheAttribute(const std::string &key, const std::string &val) override;
    std::string getCacheAttribute(const std::string &key) const override;
    bool setUserNote(const std::string &txid, const std::string &note) override;
    std::string getUserNote(const std::string &txid) const override;
    std::string getTxKey(const std::string &txid) const override;
    bool checkTxKey(const std::string &txid, std::string tx_key, const std::string &address, uint64_t &received, bool &in_pool, uint64_t &confirmations) override;
    std::string getTxProof(const std::string &txid, const std::string &address, const std::string &message) const override;
    bool checkTxProof(const std::string &txid, const std::string &address, const std::string &message, const std::string &signature, bool &good, uint64_t &received, bool &in_pool, uint64_t &confirmations) override;
    std::string getSpendProof(const std::string &txid, const std::string &message) const override;
    bool checkSpendProof(const std::string &txid, const std::string &message, const std::string &signature, bool &good) const override;
    std::string getReserveProof(bool all, uint32_t account_index, uint64_t amount, const std::string &message) const override;
    bool checkReserveProof(const std::string &address, const std::string &message, const std::string &signature, bool &good, uint64_t &total, uint64_t &spent) const override;
    std::string signMessage(const std::string &message, const std::string &address) override;
    bool verifySignedMessage(const std::string &message, const std::string &address, const std::string &signature) const override;
    bool parse_uri(const std::string &uri, std::string &address, std::string &payment_id, uint64_t &amount, std::string &tx_description, std::string &recipient_name, std::vector<std::string> &unknown_parameters, std::string &error) override;
    std::string make_uri(const std::string &address, const std::string &payment_id, uint64_t amount, const std::string &tx_description, const std::string &recipient_name, std::string &error) const override;
    std::string getDefaultDataDir() const override;
    bool rescanSpent() override;
    void setOffline(bool offline) override;
    bool isOffline() const override;
    bool blackballOutputs(const std::vector<std::string> &outputs, bool add) override;
    bool blackballOutput(const std::string &amount, const std::string &offset) override;
    bool unblackballOutput(const std::string &amount, const std::string &offset) override;
    bool getRing(const std::string &key_image, std::vector<uint64_t> &ring) const override;
    bool getRings(const std::string &txid, std::vector<std::pair<std::string, std::vector<uint64_t>>> &rings) const override;
    bool setRing(const std::string &key_image, const std::vector<uint64_t> &ring, bool relative) override;
    void segregatePreForkOutputs(bool segregate) override;
    void segregationHeight(uint64_t height) override;
    void keyReuseMitigation2(bool mitigation) override;
    bool lockKeysFile() override;
    bool unlockKeysFile() override;
    bool isKeysFileLocked() override;
    Device getDeviceType() const override;
    uint64_t coldKeyImageSync(uint64_t &spent, uint64_t &unspent) override;
    void deviceShowAddress(uint32_t accountIndex, uint32_t addressIndex, const std::string &paymentId) override;
    bool reconnectDevice() override;
    uint64_t getBytesReceived() override;
    uint64_t getBytesSent() override;

    // Definitions
    NetworkType nettype() const override {return static_cast<NetworkType>(m_wallet->nettype());}
    uint64_t getRefreshFromBlockHeight() const override { return m_wallet->get_refresh_from_block_height(); };

    // Multisig
    MultisigState multisig() const override;
    std::string getMultisigInfo() const override;
    std::string makeMultisig(const std::vector<std::string> &info, uint32_t threshold) override;
    std::string exchangeMultisigKeys(const std::vector<std::string> &info, const bool force_update_use_with_caution = false) override;
    bool exportMultisigImages(std::string &images) override;
    size_t importMultisigImages(const std::vector<std::string> &images) override;
    bool hasMultisigPartialKeyImages() const override;
    PendingTransaction * restoreMultisigTransaction(const std::string &signData) override;
    std::string signMultisigParticipant(const std::string &message) const override;
    bool verifyMessageWithPublicKey(const std::string &message, const std::string &publicKey, const std::string &signature) const override;


    // TODO : CONTINUE HERE
    // Non-override
    bool create(const std::string &path, const std::string &password,
                const std::string &language);
    bool open(const std::string &path, const std::string &password);
    bool recover(const std::string &path,const std::string &password,
                            const std::string &seed, const std::string &seed_offset = {});
    bool recoverFromKeysWithPassword(const std::string &path,
                            const std::string &password,
                            const std::string &language,
                            const std::string &address_string,
                            const std::string &viewkey_string,
                            const std::string &spendkey_string = "");
    // following two methods are deprecated since they create passwordless wallets
    // use the two equivalent methods above
    bool recover(const std::string &path, const std::string &seed);
    // deprecated: use recoverFromKeysWithPassword() instead
    bool recoverFromKeys(const std::string &path,
                            const std::string &language,
                            const std::string &address_string,
                            const std::string &viewkey_string,
                            const std::string &spendkey_string = "");
    bool recoverFromDevice(const std::string &path,
                           const std::string &password,
                           const std::string &device_name);
    bool close(bool store = true);
    // void setListener(Listener *) {}

private:
    void clearStatus() const;
    void setStatusError(const std::string& message) const;
    void setStatusCritical(const std::string& message) const;
    void setStatus(int status, const std::string& message) const;
    void refreshThreadFunc();
    void doRefresh();
    bool daemonSynced() const;
    void stopRefresh();
    bool isNewWallet() const;
    void pendingTxPostProcess(PendingTransactionImpl * pending);
    bool doInit(const std::string &daemon_address, const std::string &proxy_address, uint64_t upper_transaction_size_limit = 0, bool ssl = false);

private:
    friend class PendingTransactionImpl;
    friend class UnsignedTransactionImpl;
    friend class TransactionHistoryImpl;
    friend struct Wallet2CallbackImpl;
    friend class AddressBookImpl;
    friend class SubaddressImpl;
    friend class SubaddressAccountImpl;
    friend class ::WalletApiAccessorTest;

    std::unique_ptr<tools::wallet2> m_wallet;
    mutable boost::mutex m_statusMutex;
    mutable int m_status;
    mutable std::string m_errorString;
    std::string m_password;
    std::unique_ptr<TransactionHistoryImpl> m_history;
    std::unique_ptr<Wallet2CallbackImpl> m_wallet2Callback;
    std::unique_ptr<AddressBookImpl>  m_addressBook;
    std::unique_ptr<SubaddressImpl>  m_subaddress;
    std::unique_ptr<SubaddressAccountImpl>  m_subaddressAccount;

    // multi-threaded refresh stuff
    std::atomic<bool> m_refreshEnabled;
    std::atomic<bool> m_refreshThreadDone;
    std::atomic<int>  m_refreshIntervalMillis;
    std::atomic<bool> m_refreshShouldRescan;
    // synchronizing  refresh loop;
    boost::mutex        m_refreshMutex;

    // synchronizing  sync and async refresh
    boost::mutex        m_refreshMutex2;
    boost::condition_variable m_refreshCV;
    boost::thread       m_refreshThread;
    // flag indicating wallet is recovering from seed
    // so it shouldn't be considered as new and pull blocks (slow-refresh)
    // instead of pulling hashes (fast-refresh)
    std::atomic<bool>   m_recoveringFromSeed;
    std::atomic<bool>   m_recoveringFromDevice;
    std::atomic<bool>   m_synchronized;
    std::atomic<bool>   m_rebuildWalletCache;
    // cache connection status to avoid unnecessary RPC calls
    mutable std::atomic<bool>   m_is_connected;
    boost::optional<epee::net_utils::http::login> m_daemon_login{};
};


} // namespace

#endif
