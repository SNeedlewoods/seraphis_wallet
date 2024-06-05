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

#pragma once


#include <string>
#include <vector>
#include <list>
#include <set>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <cstdint>

//  Public interface for libwallet library
namespace Monero {

enum NetworkType : uint8_t {
    MAINNET = 0,
    TESTNET,
    STAGENET
};

    namespace Utils {
        bool isAddressLocal(const std::string &hostaddr);
        void onStartup();
    }

    template<typename T>
    class optional {
      public:
        optional(): set(false) {}
        optional(const T &t): t(t), set(true) {}
        const T &operator*() const { return t; }
        T &operator*() { return t; }
        operator bool() const { return set; }
      private:
        T t;
        bool set;
    };

/**
* brief: Transaction-like interface for sending money
*/
struct PendingTransaction
{
    enum Status {
        Status_Ok,
        Status_Error,
        Status_Critical
    };

    enum Priority {
        Priority_Default = 0,
        Priority_Low = 1,
        Priority_Medium = 2,
        Priority_High = 3,
        Priority_Last
    };

    virtual ~PendingTransaction() = 0;
    // TODO : since this is DEPRECATED in `Wallet` maybe we should add `statusWithErrorString()` and deprecate `status()` and `errorString()` here too
    /**
    * brief: status -
    * return: pending transaction status (Status_Ok | Status_Error | Status_Critical)
    */
    virtual int status() const = 0;
    /**
    * brief: errorString -
    * return: error string in case of error status
    */
    virtual std::string errorString() const = 0;
    /**
    * brief: commit - commit transaction or save it to a file if filename is provided
    * param: filename  -
    * param: overwrite -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool commit(const std::string &filename = "", bool overwrite = false) = 0;
    /**
    * brief: amount -
    * return: total amount in `m_pending_tx` vector for all destinations
    */
    virtual uint64_t amount() const = 0;
    /**
    * brief: dust -
    * return: total dust in `m_pending_tx` vector
    */
    virtual uint64_t dust() const = 0;
    /**
    * brief: fee -
    * return: total fee in `m_pending_tx` vector
    */
    virtual uint64_t fee() const = 0;
    /**
    * brief: txid -
    * return: all tx ids in `m_pending_tx` vector
    */
    virtual std::vector<std::string> txid() const = 0;
    // TODO : investigate how `m_pending_tx` gets set, I think this description may be wrong (or not accurate), otherwise we wouldn't need a vector for subaddrAccount (single account would be enough)
    /**
    * brief: txCount - get number of transactions current transaction will be splitted to
    * return: size of `m_pending_tx` vector
    */
    virtual uint64_t txCount() const = 0;
    /**
    * brief: subaddrAccount - get major indices for accounts used in tx
    * return: account indices
    */
    virtual std::vector<uint32_t> subaddrAccount() const = 0;
    /**
    * brief: subaddrIndices - get a set of minor indices used as inputs, per tx in `m_pending_tx`
    * return: subaddress indices
    */
    virtual std::vector<std::set<uint32_t>> subaddrIndices() const = 0;

    /**
    * brief: multisigSignData - initialize multisig tx
    *         Transfer this data to another wallet participant to sign it.
    *         Assumed use case is:
    *         1. Initiator:
    *              auto data = pendingTransaction->multisigSignData();
    *         2. Signer1:
    *              pendingTransaction = wallet->restoreMultisigTransaction(data);
    *              pendingTransaction->signMultisigTx();
    *              auto signed = pendingTransaction->multisigSignData();
    *         3. Signer2:
    *              pendingTransaction = wallet->restoreMultisigTransaction(signed);
    *              pendingTransaction->signMultisigTx();
    *              pendingTransaction->commit();
    * return: encoded multisig transaction with signers' keys if succeeded, else empty string
    * note: sets status error on fail
    */
    virtual std::string multisigSignData() = 0;
    /**
    * brief: signMultisigTx - sign initialized multisig tx
    *                         (see `multisigSignData()`)
    * note: sets status error on fail
    */
    virtual void signMultisigTx() = 0;
    /**
    * brief: signersKeys - get public keys from multisig tx signers
    * return: base58-encoded signers' public keys
    */
    virtual std::vector<std::string> signersKeys() const = 0;
};

/**
* brief: Transaction-like interface for sending money
*/
struct UnsignedTransaction
{
    enum Status {
        Status_Ok,
        Status_Error,
        Status_Critical
    };

    virtual ~UnsignedTransaction() = 0;
    // TODO : since this is DEPRECATED in `Wallet` maybe we should add `statusWithErrorString()` and deprecate `status()` and `errorString()` here too
    /**
    * brief: status -
    * return: wallet status (Status_Ok | Status_Error | Status_Critical)
    */
    virtual int status() const = 0;
    /**
    * brief: errorString -
    * return: error string in case of error status
    */
    virtual std::string errorString() const = 0;
    /**
    * brief: amount -
    * return: amounts per destination per tx in `m_unsigned_tx_set`
    */
    virtual std::vector<uint64_t> amount() const = 0;
    /**
    * brief: fee -
    * return: fees per tx in `m_unsigned_tx_set`
    */
    virtual std::vector<uint64_t>  fee() const = 0;
    // TODO : deprecated!?
    /**
    * brief: mixin -
    * return: minimum amount of decoy ring members per tx in `m_unsigned_tx_set`
    */
    virtual std::vector<uint64_t> mixin() const = 0;
    /**
    * brief: confirmationMessage - get `m_confirmationMessage`, which gets set in `WalletImpl::loadUnsignedTx()`->`UnsingedTransactionImpl::checkLoadedTx()`
    * return: information about all transactions
    */
    virtual std::string confirmationMessage() const = 0;
    /**
    * brief: paymentId -
    * return: vector of hexadecimal payment id (if given) or empty string per tx in `m_unsigned_tx_set`
    */
    virtual std::vector<std::string> paymentId() const = 0;
    // TODO : investigate if this is what we want, or do we need to add a loop in `recipientAddress()` for "per destination"
    /**
    * brief: recipientAddress -
    * return: first destination address per tx in `m_unsigned_tx_set`
    */
    virtual std::vector<std::string> recipientAddress() const = 0;
    // TODO : deprecated?
    /**
    * brief: minMixinCount -
    * return: minimum amount of decoy ring members from all txs in `m_unsigned_tx_set`
    */
    virtual uint64_t minMixinCount() const = 0;
    // TODO : investigate, I think this description may be wrong (or not accurate)
    /**
    * brief: txCount - get number of transactions current transaction will be splitted to
    * return: size of `m_unsigned_tx_set.txes` vector
    */
    virtual uint64_t txCount() const = 0;
    /**
    * brief: sign - sign tx and save it to file
    * param: signedFileName -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool sign(const std::string &signedFileName) = 0;
};

/**
* brief: TransactionInfo - interface for displaying transaction information
*                          used in `TransactionHistory`
*/
struct TransactionInfo
{
    enum Direction {
        Direction_In,
        Direction_Out
    };

    struct Transfer {
        Transfer(uint64_t _amount, const std::string &address);
        const uint64_t amount;
        const std::string address;
    };

    virtual ~TransactionInfo() = 0;
    // Getter functions
    // TODO : consider to return `Direction` instead of `int`
    virtual int  direction() const = 0;     // return: 0 = in, 1 = out
    virtual bool isPending() const = 0;
    virtual bool isFailed() const = 0;
    virtual bool isCoinbase() const = 0;
    virtual uint64_t amount() const = 0;
    virtual uint64_t fee() const = 0;       // return: fee if `Direction_Out`, else 0
    virtual uint64_t blockHeight() const = 0;
    virtual std::string description() const = 0;
    virtual std::set<uint32_t> subaddrIndex() const = 0;
    virtual uint32_t subaddrAccount() const = 0;
    virtual std::string label() const = 0;
    virtual uint64_t confirmations() const = 0;
    virtual uint64_t unlockTime() const = 0;
    virtual std::string hash() const = 0;   // return: transaction id
    virtual std::time_t timestamp() const = 0;
    virtual std::string paymentId() const = 0;
    //! only applicable for output transactions
    virtual const std::vector<Transfer> & transfers() const = 0;
};

/**
* brief: TransactionHistory - interface for displaying transaction history
*/
struct TransactionHistory
{
    virtual ~TransactionHistory() = 0;
    /**
    * brief: count -
    * return: amount of transactions in this history
    */
    virtual int count() const = 0;
    /**
    * brief: transaction - get info by index
    * param: index - index of tx info in this history
    * return: transaction info if available for given index, else nullptr
    */
    virtual TransactionInfo * transaction(int index)  const = 0;
    /**
    * brief: transaction - get info by tx id
    * param: id - tx id
    * return: transaction info if available for given id, else nullptr
    */
    virtual TransactionInfo * transaction(const std::string &id) const = 0;
    /**
    * brief: getAll - get every transaction info in this history
    * return: transaction info
    */
    virtual std::vector<TransactionInfo*> getAll() const = 0;
    /**
    * brief: refresh - clear history and create a new one from wallet cache data
    */
    virtual void refresh() = 0;
    // TODO : not so sure here is the best place for this function
    /**
    * brief: setTxNote - set a note for tx with given txid, only for wallet owner
    * param: txid -
    * param: note -
    */
    virtual void setTxNote(const std::string &txid, const std::string &note) = 0;
};

/**
* brief: AddressBookRow - provides an interface for address book entries
*/
struct AddressBookRow {
public:
    AddressBookRow(std::size_t _rowId, const std::string &_address, const std::string &_paymentId, const std::string &_description):
        m_rowId(_rowId),
        m_address(_address),
        m_paymentId(_paymentId), 
        m_description(_description) {}

    std::size_t getRowId() const {return m_rowId;}
    std::string getAddress() const {return m_address;}
    std::string getPaymentId() const {return m_paymentId;} // OBSOLETE
    std::string getDescription() const {return m_description;}
 
public:
    std::string extra; // TODO : it seems this is not used in the current implementation, can we remove it?
private:
    std::size_t m_rowId;
    std::string m_address;
    std::string m_paymentId; // OBSOLETE
    std::string m_description;
};

/**
* brief: AddressBook - provides functions to manage address book
*/
struct AddressBook
{
    enum ErrorCode {
        Status_Ok,
        General_Error,
        Invalid_Address,
        Invalid_Payment_Id
    };
    virtual ~AddressBook() = 0;
    /**
    * brief: getAll -
    * return: all address book rows/entries
    */
    virtual std::vector<AddressBookRow*> getAll() const = 0;
    /**
    * brief: addRow      - add new entry to address book
    * param: dst_addr    - destination address
    * param: payment_id  - OBSOLETE
    * param: description - arbitrary note
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool addRow(const std::string &dst_addr , const std::string &payment_id, const std::string &description) = 0;  
    /**
    * brief: deleteRow - remove entry from address book
    * param: rowId - index
    * return: true if succeeded
    */
    virtual bool deleteRow(std::size_t rowId) = 0;
    /**
    * brief: setDescription - set or change description of address book entry
    * param: index -
    * param: description - arbitrary note
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool setDescription(std::size_t index, const std::string &description) = 0;
    /**
    * brief: refresh - clear address book entries and create new ones from wallet cache data
    * param: index -
    * param: description - arbitrary note
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual void refresh() = 0;  
    // TODO : since this is DEPRECATED in `Wallet` maybe we should add `statusWithErrorString()` and deprecate `errorCode()` and `errorString()` here too
    /**
    * brief: errorString -
    * return: error string in case of error status
    */
    virtual std::string errorString() const = 0;
    /**
    * brief: errorCode -
    * return: address book status (see `ErrorCode` above)
    */
    virtual int errorCode() const = 0;
    /**
    * brief: lookupPaymentID - get row index of address book entry with given payment_id
    * param: payment_id -
    * return: row index if succeeded, else -1
    * note: OBSOLETE - payment id now is part of integrated addresses
    */
    virtual int lookupPaymentID(const std::string &payment_id) const = 0;
};

// TODO CONTINUE HERE
struct SubaddressRow {
public:
    SubaddressRow(std::size_t _rowId, const std::string &_address, const std::string &_label):
        m_rowId(_rowId),
        m_address(_address),
        m_label(_label) {}
 
private:
    std::size_t m_rowId;
    std::string m_address;
    std::string m_label;
public:
    std::string extra;
    std::string getAddress() const {return m_address;}
    std::string getLabel() const {return m_label;}
    std::size_t getRowId() const {return m_rowId;}
};

struct Subaddress
{
    virtual ~Subaddress() = 0;
    virtual std::vector<SubaddressRow*> getAll() const = 0;
    virtual void addRow(uint32_t accountIndex, const std::string &label) = 0;
    virtual void setLabel(uint32_t accountIndex, uint32_t addressIndex, const std::string &label) = 0;
    virtual void refresh(uint32_t accountIndex) = 0;
};

struct SubaddressAccountRow {
public:
    SubaddressAccountRow(std::size_t _rowId, const std::string &_address, const std::string &_label, const std::string &_balance, const std::string &_unlockedBalance):
        m_rowId(_rowId),
        m_address(_address),
        m_label(_label),
        m_balance(_balance),
        m_unlockedBalance(_unlockedBalance) {}

private:
    std::size_t m_rowId;
    std::string m_address;
    std::string m_label;
    std::string m_balance;
    std::string m_unlockedBalance;
public:
    std::string extra;
    std::string getAddress() const {return m_address;}
    std::string getLabel() const {return m_label;}
    std::string getBalance() const {return m_balance;}
    std::string getUnlockedBalance() const {return m_unlockedBalance;}
    std::size_t getRowId() const {return m_rowId;}
};

struct SubaddressAccount
{
    virtual ~SubaddressAccount() = 0;
    virtual std::vector<SubaddressAccountRow*> getAll() const = 0;
    virtual void addRow(const std::string &label) = 0;
    virtual void setLabel(uint32_t accountIndex, const std::string &label) = 0;
    virtual void refresh() = 0;
};

struct MultisigState {
    MultisigState() : isMultisig(false), isReady(false), threshold(0), total(0) {}

    bool isMultisig;
    bool isReady;
    uint32_t threshold;
    uint32_t total;
};


struct DeviceProgress {
    DeviceProgress(): m_progress(0), m_indeterminate(false) {}
    DeviceProgress(double progress, bool indeterminate=false): m_progress(progress), m_indeterminate(indeterminate) {}

    virtual double progress() const { return m_progress; }
    virtual bool indeterminate() const { return m_indeterminate; }

protected:
    double m_progress;
    bool m_indeterminate;
};

struct Wallet;
struct WalletListener
{
    virtual ~WalletListener() = 0;
    /**
     * @brief moneySpent - called when money spent
     * @param txId       - transaction id
     * @param amount     - amount
     */
    virtual void moneySpent(const std::string &txId, uint64_t amount) = 0;

    /**
     * @brief moneyReceived - called when money received
     * @param txId          - transaction id
     * @param amount        - amount
     */
    virtual void moneyReceived(const std::string &txId, uint64_t amount) = 0;
    
   /**
    * @brief unconfirmedMoneyReceived - called when payment arrived in tx pool
    * @param txId          - transaction id
    * @param amount        - amount
    */
    virtual void unconfirmedMoneyReceived(const std::string &txId, uint64_t amount) = 0;

    /**
     * @brief newBlock      - called when new block received
     * @param height        - block height
     */
    virtual void newBlock(uint64_t height) = 0;

    /**
     * @brief updated  - generic callback, called when any event (sent/received/block reveived/etc) happened with the wallet;
     */
    virtual void updated() = 0;


    /**
     * @brief refreshed - called when wallet refreshed by background thread or explicitly refreshed by calling "refresh" synchronously
     */
    virtual void refreshed() = 0;

    /**
     * @brief called by device if the action is required
     */
    virtual void onDeviceButtonRequest(uint64_t code) { (void)code; }

    /**
     * @brief called by device if the button was pressed
     */
    virtual void onDeviceButtonPressed() { }

    /**
     * @brief called by device when PIN is needed
     */
    virtual optional<std::string> onDevicePinRequest() {
        throw std::runtime_error("Not supported");
    }

    /**
     * @brief called by device when passphrase entry is needed
     */
    virtual optional<std::string> onDevicePassphraseRequest(bool & on_device) {
        on_device = true;
        return optional<std::string>();
    }

    /**
     * @brief Signalizes device operation progress
     */
    virtual void onDeviceProgress(const DeviceProgress & event) { (void)event; };

    /**
     * @brief If the listener is created before the wallet this enables to set created wallet object
     */
    virtual void onSetWallet(Wallet * wallet) { (void)wallet; };
};


/**
* brief: Interface for wallet operations. Implementation can be found in `src/wallet/api/wallet[.h/.cpp]`.
*/
struct Wallet
{
    enum Device {
        Device_Software = 0,
        Device_Ledger = 1,
        Device_Trezor = 2
    };

    enum Status {
        Status_Ok,
        Status_Error,
        Status_Critical
    };

    enum ConnectionStatus {
        ConnectionStatus_Disconnected,
        ConnectionStatus_Connected,
        ConnectionStatus_WrongVersion
    };

    // Overrides
    virtual ~Wallet() = 0;
    /**
    * brief: seed           - get mnemonic seed phrase for deterministic wallet
    * param: seed_offset    - passphrase
    * return: mnemonic seed phrase
    */
    virtual std::string seed(const std::string& seed_offset = "") const = 0;
    /**
    * brief: getSeedLanguage - get language used for mnemonic seed phrase
    * return: seed phrase language
    */
    virtual std::string getSeedLanguage() const = 0;
    /**
    * brief: setSeedLanguage - set language used for mnemonic seed phrase
    * param: arg             - seed phrase language
    */
    virtual void setSeedLanguage(const std::string &arg) = 0;
    /**
    * brief: status -
    * return: wallet status (Status_Ok | Status_Error | Status_Critical)
    * note: DEPRECATED - use safe alternative statusWithErrorString
    */
    virtual int status() const = 0;
    /**
    * brief: errorString -
    * return: error string in case of error status
    * note: DEPRECATED - use safe alternative statusWithErrorString
    */
    virtual std::string errorString() const = 0;
    /**
    * brief: statusWithErrorString -
    * outparam: status      - (Status_Ok | Status_Error | Status_Critical)
    * outparam: errorString - set in case of error status
    */
    virtual void statusWithErrorString(int& status, std::string& errorString) const = 0;
    /**
    * brief: setPassword -
    * param: password    - new password
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool setPassword(const std::string &password) = 0;
    /**
    * brief: getPassword -
    * return: password
    */
    virtual const std::string& getPassword() const = 0;
    /**
    * brief: setDevicePin -
    * param: pin          - new pin
    * return: true if succeeded
    * note: sets status error on fail
    */
    // TODO : figure out why we use `{ (void)pin; return false; }` instead of `= 0` here
    virtual bool setDevicePin(const std::string &pin) { (void)pin; return false; };
    /**
    * brief: setDevicePassphrase -
    * param: passphrase - new pin
    * return: true if succeeded
    * note: sets status error on fail
    */
    // TODO : figure out why we use `{ (void)passphrase; return false; }` instead of `= 0` here
    virtual bool setDevicePassphrase(const std::string &passphrase) { (void)passphrase; return false; };
    /**
    * brief: address      - get subaddress
    * param: accountIndex - major index
    * param: addressIndex - minor index
    * return: subaddress
    */
    virtual std::string address(uint32_t accountIndex = 0, uint32_t addressIndex = 0) const = 0;
    /**
    * brief: mainAddress  - get standard address with both major and minor index = 0
    * return: standard address
    */
    std::string mainAddress() const { return address(0, 0); }
    /**
    * brief: path - get path to wallet file
    * return: path
    */
    virtual std::string path() const = 0;
    /**
    * brief: nettype - get wallets network type (mainnet | testnet | stagenet)
    * return: network type
    */
    virtual NetworkType nettype() const = 0;
    bool mainnet() const { return nettype() == MAINNET; }
    bool testnet() const { return nettype() == TESTNET; }
    bool stagenet() const { return nettype() == STAGENET; }
    /**
    * brief: hardForkInfo - get earliest height for requested version from RPC `hard_fork_info`
    * param: version      - hard-fork version
    * outparam: earliest_height -
    */
    virtual void hardForkInfo(uint8_t version, uint64_t &earliest_height) const = 0;
    /**
    * brief: useForkRules - check if hard fork rules should be used
    * param: version      - hard-fork version
    * param: early_blocks - minimum distance to earliest hard-fork height
    * return: true if hard fork rules should be used
    */
    virtual bool useForkRules(uint8_t version, int64_t early_blocks = 0) const = 0;
    // TODO : This description does not match what the function actually does. Either remove this comment or change the implementation in `api/wallet.cpp` and remove the comment below
    /**
    * brief: integratedAddress - get integrated address for current wallet address and given payment_id
    *                            generate random payment_id if given payment_id is empty or not-valid
    * param: payment_id        - 16 character hexadecimal string or empty string for new random payment id
    * return: integrated address
    */
    /**
    * brief: integratedAddress - get integrated address for current wallet address and given payment_id
    * param: payment_id        - 16 character hexadecimal string
    * return: integrated address if succeeded, else empty string
    */
    virtual std::string integratedAddress(const std::string &payment_id) const = 0;
    /**
    * brief: secretViewKey -
    * return: secret view key
    */
    virtual std::string secretViewKey() const = 0;
    /**
    * brief: publicViewKey -
    * return: public view key
    */
    virtual std::string publicViewKey() const = 0;
    /**
    * brief: secretSpendKey -
    * return: secret spend key
    */
    virtual std::string secretSpendKey() const = 0;
    /**
    * brief: publicSpendKey -
    * return: public spend key
    */
    virtual std::string publicSpendKey() const = 0;
    /**
    * brief: publicMultisigSignerKey -
    * return: public multisignature signer key or empty string if wallet is not multisig
    */
    virtual std::string publicMultisigSignerKey() const = 0;
    /**
    * brief: stop - interrupts wallet refresh() loop once (doesn't stop background refresh thread)
    */
    virtual void stop() = 0;
    /**
    * brief: store - store wallet_file, address_file and keys_file
    * param: path  - path ending in wallet_file name
    *                empty string to store to current wallet_file
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool store(const std::string &path) = 0;
    /**
    * brief: filename - get wallet_file name
    * return: path ending in wallet_file name
    */
    virtual std::string filename() const = 0;
    /**
    * brief: keysFilename - get keys_file name (wallet_file +".keys")
    * return: path ending in keys_file name
    */
    virtual std::string keysFilename() const = 0;
    /**
    * brief: init   - initializes wallet with daemon connection params.
    *                 if daemon_address is local address, "trusted daemon" will be set to true forcibly
    *                 startRefresh() should be called when wallet is initialized.
    * param: daemon_address  - in "hostname:port" format
    * param: upper_transaction_size_limit -
    * param: daemon_username -
    * param: daemon_password -
    * param: lightWallet     - DEPRECATED
    * param: proxy_address   - empty string to disable proxy
    * return: true if succeeded
    */
    virtual bool init(const std::string &daemon_address, uint64_t upper_transaction_size_limit = 0, const std::string &daemon_username = "", const std::string &daemon_password = "", bool use_ssl = false, bool lightWallet = false, const std::string &proxy_address = "") = 0;
    // TODO NEXT : look into createMultisig and createNonDeterministic
    //              though I think createWalletFromKeys in WalletManager can do both, non-deterministic and watch-only
    /**
    * brief: create   - create new wallet
    * param: path     - path ending in wallet_file name
    * param: password - wallet password
    * param: language - mnemonic seed language
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool create(const std::string &path, const std::string &password, const std::string &language) = 0;
    /**
    * brief: createWatchOnly - create new watch only wallet from current wallet's view key
    * param: path     - path ending in wallet_file name
    * param: password - wallet password
    * param: language - mnemonic seed language
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool createWatchOnly(const std::string &path, const std::string &password, const std::string &language) const = 0;
    /**
    * brief: recover     - recover wallet from mnemonic seed phrase
    * param: path        - path ending in wallet_file name
    * param: password    - wallet password
    * param: seed        - mnemonic seed phrase
    * param: seed_offset - mnemonic seed phrase offset password
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool recover(const std::string &path, const std::string &password, const std::string &seed, const std::string &seed_offset = "") = 0;
    bool recover(const std::string &path, const std::string &seed) { return recover(path, "", seed); } // deprecated
    /**
    * brief: recoverFromKeysWithPassword - recover wallet from keys
    * param: path            - path ending in wallet_file name
    * param: password        - wallet password
    * param: address_string  - wallet main address (TODO : shouldn't this be optional and only nedded for non-deterministic or view-only?)
    * param: viewkey_string  - needed for non-deterministic or view-only wallet
    * param: spendkey_string - needed for non-deterministic or deterministic wallet
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool recoverFromKeysWithPassword(const std::string &path,
                            const std::string &password,
                            const std::string &language,
                            const std::string &address_string,
                            const std::string &viewkey_string,
                            const std::string &spendkey_string = "") = 0;
    bool recoverFromKeys(const std::string &path,
                         const std::string &language,
                         const std::string &address_string,
                         const std::string &viewkey_string,
                         const std::string &spendkey_string) // deprecated
    {
        return recoverFromKeysWithPassword(path, "", language, address_string, viewkey_string, spendkey_string);
    }
    /**
    * brief: recoverFromDevice - recover wallet from hardware device
    * param: path        - path ending in wallet_file name
    * param: password    - wallet password
    * param: device_name -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool recoverFromDevice(const std::string &path, const std::string &password, const std::string &device_name) = 0;
    /**
    * brief: open - load wallet from wallet file and keys file
    * param: path     - path ending in wallet_file name
    * param: password - wallet password
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool open(const std::string &path, const std::string &password) = 0;
    /**
    * brief: close - close wallet
    * param: store - true if wallet should get stored before closing
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool close(bool store = true) = 0;
    /**
    * brief: setRefreshFromBlockHeight - set wallet restore height
    * param: refresh_from_block_height - restore height
    */
    virtual void setRefreshFromBlockHeight(uint64_t refresh_from_block_height) = 0;
    /**
    * brief: getRefreshFromBlockHeight -
    * return: restore height / wallet creation height
    */
    virtual uint64_t getRefreshFromBlockHeight() const = 0;
    /**
    * brief: setRecoveringFromSeed -
    * param: recoveringFromSeed -
    */
    virtual void setRecoveringFromSeed(bool recoveringFromSeed) = 0;
    /**
    * brief: setRecoveringFromDevice -
    * param: recoveringFromDevice -
    */
    virtual void setRecoveringFromDevice(bool recoveringFromDevice) = 0;
    /**
    * brief: setSubaddressLookahead - set range for subaddresses which will be used for scanning
    * param: major - account index
    * param: minor - subaddress index
    */
    virtual void setSubaddressLookahead(uint32_t major, uint32_t minor) = 0;
    /**
    * brief: connectToDaemon -
    * return: true if succeeded or if already connected
    */
    virtual bool connectToDaemon() = 0;
    /**
    * brief: connected - check if wallet is connected to a daemon and try to connect if disconnected
    * return: connection status (disconnected | connected | wrong version)
    * note: sets status error on fail
    */
    virtual ConnectionStatus connected() const = 0;
    /**
    * brief: setTrustedDaemon - set trusted daemon setting
    * param: arg - is daemon trusted
    */
    virtual void setTrustedDaemon(bool arg) = 0;
    /**
    * brief: trustedDaemon - get trusted daemon setting
    * return: true if daemon is trusted
    */
    virtual bool trustedDaemon() const = 0;
    /**
    * brief: setProxy - set proxy for daemon connection
    * param: address  - in "hostname:port" format
    * return: true if succeeded
    */
    virtual bool setProxy(const std::string &address) = 0;
    /**
    * brief: balance      - get account balance
    * param: accountIndex - major index
    * return: balance
    */
    virtual uint64_t balance(uint32_t accountIndex = 0) const = 0;
    /**
    * brief: balanceAll - get total balance
    * return: total balance from all accounts
    */
    uint64_t balanceAll() const {
        uint64_t result = 0;
        for (uint32_t i = 0; i < numSubaddressAccounts(); ++i)
            result += balance(i);
        return result;
    }
    /**
    * brief: unlockedBalance - get unlocked account balance
    * param: accountIndex    - major index
    * return: unlocked balance
    */
    virtual uint64_t unlockedBalance(uint32_t accountIndex = 0) const = 0;
    /**
    * brief: unlockedBalanceAll - get total unlocked balance
    * return: total unlocked balance from all accounts
    */
    uint64_t unlockedBalanceAll() const {
        uint64_t result = 0;
        for (uint32_t i = 0; i < numSubaddressAccounts(); ++i)
            result += unlockedBalance(i);
        return result;
    }
    /**
    * brief: watchOnly - check if wallet is watch only
    * return: true if watch only
    */
    virtual bool watchOnly() const = 0;
    /**
    * brief: isDeterministic - check if wallet keys are deterministic only
    * return: true if deterministic
    */
    virtual bool isDeterministic() const = 0;
    /**
    * brief: blockChainHeight - get current blockchain height
    * return: blockchain height
    */
    virtual uint64_t blockChainHeight() const = 0;
    /**
    * brief: approximateBlockChainHeight - get approximate blockchain height calculated from date/time
    * return: approximate blockchain height
    */
    virtual uint64_t approximateBlockChainHeight() const = 0;
    /**
    * brief: estimateBlockChainHeight - get estimate blockchain height from daemon and falls back to calculation from date/time
    *                                   more accurate than approximateBlockChainHeight
    * return: estimate blockchain height
    */
    virtual uint64_t estimateBlockChainHeight() const = 0;
    /**
    * brief: daemonBlockChainHeight - get daemon blockchain height
    * return: 0 in case of an error else daemon blockchain height
    * note: sets status error on fail
    */
    virtual uint64_t daemonBlockChainHeight() const = 0;
    /**
    * brief: daemonBlockChainTargetHeight - get daemon blockchain target height
    * return: daemon blockchain target height if not zero and no errors
    *         else daemon blockchain height
    * note: sets status error on fail
    */
    virtual uint64_t daemonBlockChainTargetHeight() const = 0;
    // TODO : This description does not match what the function actually does. Either remove this comment or change the implementation in `api/wallet.cpp` and remove the comment below
    //        except I'm missing something: m_synchronized is set to false once when wallet gets initialized
    //        and gets set to true once the daemon is synchronized with the network and wallet is synchronized with the daemon
    /**
    * brief: synchronized - check if wallet was ever synchronized
    * return: true if wallet was ever synchronized
    */
    /**
    * brief: synchronized - check if wallet was synchronized once since it was loaded
    * return: true if wallet was synchronized
    */
    virtual bool synchronized() const = 0;
    /**
    * brief: startRefresh - start/resume refresh thread (refresh every 10 seconds)
    */
    virtual void startRefresh() = 0;
    /**
    * brief: pauseRefresh - pause refresh thread
    */
    virtual void pauseRefresh() = 0;
    /**
    * brief: refresh - refresh the wallet, update transactions from daemon
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool refresh() = 0;
    /**
    * brief: refreshAsync - refresh the wallet asynchronously
    * note: sets status error on fail
    */
    virtual void refreshAsync() = 0;
    /**
    * brief: rescanBlockchain - rescan the wallet, update transactions from daemon
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool rescanBlockchain() = 0;
    /**
    * brief: rescanBlockchainAsync - rescan the wallet asynchronously
    * note: sets status error on fail
    */
    virtual void rescanBlockchainAsync() = 0;
    /**
    * brief: setAutoRefreshInterval - set interval for automatic refresh
    * param: millis - interval in milliseconds, if zero or less than zero - automatic refresh diabled
    */
    virtual void setAutoRefreshInterval(int millis) = 0;
    /**
    * brief: autoRefreshInterval - get interval for automatic refresh
    * return: interval in milliseconds
    */
    virtual int autoRefreshInterval() const = 0;
    /**
    * brief: addSubaddressAccount - appends a new subaddress account at the end of the last major index of existing subaddress accounts
    * param: label - the label for the new account
    */
    virtual void addSubaddressAccount(const std::string &label) = 0;
    /**
    * brief: numSubaddressAccounts - get the number of existing subaddress accounts
    * return: number of subaddress accounts
    */
    virtual size_t numSubaddressAccounts() const = 0;
    /**
    * brief: numSubaddresses - get the number of existing subaddresses associated with the specified subaddress account
    * param: accountIndex    - major index
    * return: number of subaddresses for specified account
    */
    virtual size_t numSubaddresses(uint32_t accountIndex) const = 0;
    /**
    * brief: addSubaddress - appends a new subaddress at the end of the last minor index of the specified subaddress account
    * param: accountIndex  - major index
    * param: label         - the label for the new subaddress
    */
    virtual void addSubaddress(uint32_t accountIndex, const std::string &label) = 0;
    /**
    * brief: getSubaddressLabel - get the label of the specified subaddress if subaddress exists
    * param: accountIndex  - major index
    * param: addressIndex  - minor index
    * return: label of specified subaddress if succeeded, else empty string
    */
    virtual std::string getSubaddressLabel(uint32_t accountIndex, uint32_t addressIndex) const = 0;
    /**
    * brief: setSubaddressLabel - set the label of the specified subaddress if subaddress exists
    * param: accountIndex  - major index
    * param: addressIndex  - minor index
    * return: label of specified subaddress if succeeded, else empty string
    */
    virtual void setSubaddressLabel(uint32_t accountIndex, uint32_t addressIndex, const std::string &label) = 0;
    /**
    * brief: createTransactionMultDest - create transaction with multiple destinations. if dst_addr is an integrated address, payment_id is ignored
    * param: dst_addr        - vector of destination address as string
    * param: payment_id      - optional payment_id, can be empty string
    * param: amount          - vector of amounts
    * param: mixin_count     - mixin count. if 0 passed, wallet will use default value - TODO : afaik this is deprecated, currently ring size is fixed to 16
    * param: priority        - fee level
    * param: subaddr_account - subaddress account from which the input funds are taken
    * param: subaddr_indices - set of subaddress indices to use for transfer or sweeping
    *                          if set empty, all are chosen when sweeping, and one or more are automatically chosen when transferring
    *                          after execution, returns the set of actually used indices
    * return: pending transaction. caller is responsible to check PendingTransaction::status() after object returned
    * note: sets status error on fail
    */
    virtual PendingTransaction * createTransactionMultDest(
                                        const std::vector<std::string> &dst_addr,
                                        const std::string &payment_id,
                                        optional<std::vector<uint64_t>> amount,
                                        uint32_t mixin_count,
                                        PendingTransaction::Priority priority = PendingTransaction::Priority_Low,
                                        uint32_t subaddr_account = 0,
                                        std::set<uint32_t> subaddr_indices = {}) = 0;
    /**
    * brief: createTransaction - create transaction. if dst_addr is an integrated address, payment_id is ignored
    * param: dst_addr        - destination address as string
    * param: payment_id      - optional payment_id, can be empty string
    * param: amount          - amount
    * param: mixin_count     - mixin count. if 0 passed, wallet will use default value - TODO : afaik this is deprecated, currently ring size is fixed to 16
    * param: priority        - fee level
    * param: subaddr_account - subaddress account from which the input funds are taken
    * param: subaddr_indices - set of subaddress indices to use for transfer or sweeping
    *                          if set empty, all are chosen when sweeping, and one or more are automatically chosen when transferring
    *                          after execution, returns the set of actually used indices
    * return: pending transaction. caller is responsible to check PendingTransaction::status() after object returned
    * note: sets status error on fail
    */
    virtual PendingTransaction * createTransaction(const std::string &dst_addr,
                                        const std::string &payment_id,
                                        optional<uint64_t> amount,
                                        uint32_t mixin_count,
                                        PendingTransaction::Priority priority = PendingTransaction::Priority_Low,
                                        uint32_t subaddr_account = 0,
                                        std::set<uint32_t> subaddr_indices = {}) = 0;
    /**
    * brief: createSweepUnmixableTransaction - create transaction with unmixable outputs
    * return: pending transaction. caller is responsible to check PendingTransaction::status() after object returned
    * note: sets status error on fail
    */
    virtual PendingTransaction * createSweepUnmixableTransaction() = 0;
    /**
    * brief: loadUnsignedTx - create transaction from unsigned tx file
    * param: unsigned_filename - name of file which contains unsigned tx
    * return: unsigned transaction. caller is responsible to check UnsignedTransaction::status() after object returned
    * note: sets status error on fail
    */
    virtual UnsignedTransaction * loadUnsignedTx(const std::string &unsigned_filename) = 0;
    /**
    * brief: submitTransaction - try send tx from signed tx file to the daemon
    * param: fileName - name of file which contains signed tx
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool submitTransaction(const std::string &fileName) = 0;
    /**
    * brief: disposeTransaction - destroy pending transaction object
    * param: t - pointer to pending transaction object, will become invalid after function returned
    */
    virtual void disposeTransaction(PendingTransaction * t) = 0;
    /**
    * brief: estimateTransactionFee -
    * param: destinations - vector consisting of <address, amount> pairs
    * param: priority     - fee level
    * return: estimated fee
    */
    virtual uint64_t estimateTransactionFee(const std::vector<std::pair<std::string, uint64_t>> &destinations, PendingTransaction::Priority priority) const = 0;
    /**
    * brief: exportKeyImages - export key images to file
    * param: filename -
    * param: all      -  export all key images if true, else only those that have not yet been exported
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool exportKeyImages(const std::string &filename, bool all = false) = 0;
    /**
    * brief: importKeyImages - import key images from file
    * param: filename -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool importKeyImages(const std::string &filename) = 0;
    /**
    * brief: exportOutputs - export outputs to file
    * param: filename -
    * param: all      -  export all outputs if true, else only those that have not yet been exported
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool exportOutputs(const std::string &filename, bool all = false) = 0;
    /**
    * brief: importOutputs - import outputs from file
    * param: filename -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool importOutputs(const std::string &filename) = 0;
    /**
    * brief: scanTransactions - scan a list of transaction ids, this operation may reveal the txids to the remote node and affect your privacy
    * param: txids - list of transaction ids
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool scanTransactions(const std::vector<std::string> &txids) = 0;
    /**
    * brief: history - get transaction history
    * return: transaction history
    */
    virtual TransactionHistory * history() = 0;
    /**
    * brief: addressBook - get address book
    * return: address book
    */
    virtual AddressBook * addressBook() = 0;
    /**
    * brief: subaddress - get subaddress
    * return: subaddress
    */
    virtual Subaddress * subaddress() = 0;
    /**
    * brief: subaddressAccount - get subaddress account
    * return: subaddress account
    */
    virtual SubaddressAccount * subaddressAccount() = 0;
    /**
    * brief: setListener - set wallet callback listener
    */
    virtual void setListener(WalletListener *l) = 0;
    // TODO : since we use a fixed ring size, this is deprecated, or not?
    /**
    * brief: defaultMixin - get number of mixins (decoys) used in transactions
    * return: number of mixins
    */
    virtual uint32_t defaultMixin() const = 0;
    // TODO : since we use a fixed ring size, this is deprecated, or not?
    /**
    * brief: setDefaultMixin - set number of mixins (decoys) to be used for new transactions
    * param: arg - number of mixins
    */
    virtual void setDefaultMixin(uint32_t arg) = 0;
    /**
    * brief: setCacheAttribute - attach an arbitrary string to a wallet cache attribute
    * param: key - attribute name
    * param: val - attribute value
    * return: true if succeeded
    */
    virtual bool setCacheAttribute(const std::string &key, const std::string &val) = 0;
    /**
    * brief: getCacheAttribute - get an arbitrary string attached to a wallet cache attribute
    * param: key - attribute name
    * return: value attached to key if exists, else empty string
    */
    virtual std::string getCacheAttribute(const std::string &key) const = 0;
    /**
    * brief: setUserNote - attach an arbitrary string note to a txid
    * param: txid - the transaction id to attach the note to
    * param: note -
    * return: true if succeeded
    */
    virtual bool setUserNote(const std::string &txid, const std::string &note) = 0;
    /**
    * brief: getUserNote - get an arbitrary string note attached to a txid
    * param: txid - the transaction id to attach the note to
    * return: note attached to txid if exists, else empty string
    */
    virtual std::string getUserNote(const std::string &txid) const = 0;
    /**
    * brief: getTxKey - get tx key and additional tx keys for given txid
    * param: txid - transaction id
    * return: hex string with tx key and additional tx keys if succeeded, else empty string
    * note: sets status error on fail
    */
    virtual std::string getTxKey(const std::string &txid) const = 0;
    /**
    * brief: checkTxKey - check tx with given id, key and additional keys and store information in outparams
    * param: txid             - transaction id
    * param: tx_key           - transaction key
    * param: address          - destination
    * outparam: received      - amount received
    * outparam: in_pool       - is tx currently in pool
    * outparam: confirmations - number of confirmations
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool checkTxKey(const std::string &txid, std::string tx_key, const std::string &address, uint64_t &received, bool &in_pool, uint64_t &confirmations) = 0;
    /**
    * brief: getTxProof - generate a tx proof (OutProofV2 | InProofV2)
    * param: txid    - transaction id
    * param: address - destination
    * param: message - challenge string (optional)
    // TODO : is this the right time to introduce the term enote, or should we stick with output as long as we don't use Seraphis code?
    * return: proof type prefix + for each enote in the proof concatenate base58 encoded [shared secret, signature], else empty string
    * note: sets status error on fail
    */
    virtual std::string getTxProof(const std::string &txid, const std::string &address, const std::string &message) const = 0;
    /**
    * brief: checkTxProof - try to verify a tx proof (OutProof | InProof)
    * param: txid      - transaction id
    * param: address   - destination
    * param: message   - challenge string
    * param: signature - the proof to verify
    * outparam: good          - true if tx proof signature is valid
    * outparam: received      - amount received
    * outparam: in_pool       - is tx currently in pool
    * outparam: confirmations - number of confirmations
    * return: true if no errors (signature can still be invalid)
    * note: sets status error on fail
    */
    virtual bool checkTxProof(const std::string &txid, const std::string &address, const std::string &message, const std::string &signature, bool &good, uint64_t &received, bool &in_pool, uint64_t &confirmations) = 0;
    /**
    * brief: getSpenProof - generate a spend proof (SpendProofV1)
    * param: txid    - transaction id
    * param: message - challenge string (optional)
    * return: proof type prefix + for each enote in the proof concatenate base58 encoded signature, else empty string
    * note: sets status error on fail
    */
    virtual std::string getSpendProof(const std::string &txid, const std::string &message) const = 0;
    /**
    * brief: checkSpendProof - try to verify a spend proof (SpendProof)
    * param: txid      - transaction id
    * param: message   - challenge string
    * param: signature - the proof to verify
    * outparam: good   - true if tx proof signature is valid
    * return: true if no errors (signature can still be invalid)
    * note: sets status error on fail
    */
    virtual bool checkSpendProof(const std::string &txid, const std::string &message, const std::string &signature, bool &good) const = 0;
    /**
    * brief: getReserveProof - generate a proof that proves the reserve of unspent funds (ReserveProofV2)
    * param: all           - `account_index` and `amount` are ignored if true
    * param: account_index - major index of account to restrict the proof on
    * param: amount        - minimum reserve amount
    * param: message - challenge string (optional)
    * return: proof type prefix + base58 encoded signature, else empty string
    * note: sets status error on fail
    */
    virtual std::string getReserveProof(bool all, uint32_t account_index, uint64_t amount, const std::string &message) const = 0;
    /**
    * brief: checkReserveProof - try to verify a reserve proof (ReserveProof)
    * param: address   - main address of reserve owner
    * param: message   - challenge string
    * param: signature - the proof to verify
    * outparam: good   - true if tx proof signature is valid
    * outparam: total  - total proven amount
    * outparam: spent  - TODO : this needs further investigation, on first sight why we would include spent key images if our goal is to prove that we have not spent them?
    * return: true if no errors (signature can still be invalid)
    * note: sets status error on fail
    */
    virtual bool checkReserveProof(const std::string &address, const std::string &message, const std::string &signature, bool &good, uint64_t &total, uint64_t &spent) const = 0;
    /**
    * brief: signMessage - sign a message with the spend private key (SigV2)
    * param: message - message to sign (arbitrary byte data)
    * param: address - address used to sign the message (use main address if empty)
    * return: proof type prefix + base58 encoded signature, else empty string
    * note: sets status error on fail
    */
    virtual std::string signMessage(const std::string &message, const std::string &address = "") = 0;
    /**
    * brief: verifySignedMessage - try to verify a signature was generated from given message and address
    * param: message   - message to verify
    * param: address   - address used to sign the message
    * param: signature - the proof to verify
    * return: true if signature is valid, else and in case of error return false
    */
    virtual bool verifySignedMessage(const std::string &message, const std::string &addres, const std::string &signature) const = 0;
    /**
    * brief: parse_uri - parse a monero uri and store the contained informations in outparams
    * param: uri                  - uniform resource identifier
    * outparam: address           - receiver address
    * outparam: payment_id        -
    * outparam: amount            -
    * outparam: tx_description    -
    * outparam: recipient_name    -
    * outparam: unkown_parameters -
    * outparam: error             - error message if error occurred
    * return: true if succeeded
    */
    virtual bool parse_uri(const std::string &uri, std::string &address, std::string &payment_id, uint64_t &amount, std::string &tx_description, std::string &recipient_name, std::vector<std::string> &unknown_parameters, std::string &error) = 0;
    /**
    * brief: make_uri - make a monero uri
    * param: address           - receiver address
    * param: payment_id        -
    * param: amount            -
    * param: tx_description    -
    * param: recipient_name    -
    * outparam: error          - error message if error occurred
    * return: uri if succeeded, else empty string
    */
    virtual std::string make_uri(const std::string &address, const std::string &payment_id, uint64_t amount, const std::string &tx_description, const std::string &recipient_name, std::string &error) const = 0;
    /**
    * brief: getDefaultDataDir - path to `.bitmonero` dir
    * return: default data dir
    */
    virtual std::string getDefaultDataDir() const = 0;
    /**
    * brief: rescanSpent - rescan spent outputs, can only be used with trusted daemon
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool rescanSpent() = 0;
    /**
    * brief: setOffline -
    * param: offline -
    */
    virtual void setOffline(bool offline) = 0;
    /**
    * brief: isOffline -
    * return: true if wallet is offline
    */
    virtual bool isOffline() const = 0;
    // TODO : revisit blackball and ring stuff
    /**
    * brief: blackballOutputs - mark outputs as spent
    * param: outputs - set of outputs in [amount, offset] pairs
    * param: add     - add to current set of blackballed outputs if true, else clear the current set
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool blackballOutputs(const std::vector<std::string> &outputs, bool add) = 0;
    /**
    * brief: blackballOutput - mark output as spent
    * param: amount -
    * param: offset -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool blackballOutput(const std::string &amount, const std::string &offset) = 0;
    /**
    * brief: unblackballOutput - mark output as unspent
    * param: amount -
    * param: offset -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool unblackballOutput(const std::string &amount, const std::string &offset) = 0;
    /**
    * brief: getRing   - get the ring used for a key image
    * param: key_image -
    * outparam: ring   -
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool getRing(const std::string &key_image, std::vector<uint64_t> &ring) const = 0;
    /**
    * brief: getRings  - get the rings used for a txid
    * param: key_image -
    * outparam: rings  - set of pairs of hex encoded key images with corresponding rings
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool getRings(const std::string &txid, std::vector<std::pair<std::string, std::vector<uint64_t>>> &rings) const = 0;
    /**
    * brief: setRing   - set the ring used for a key image
    * param: key_image -
    * param: ring      -
    * param: relative  - relative if true, else absolute
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool setRing(const std::string &key_image, const std::vector<uint64_t> &ring, bool relative) = 0;
    /**
    * brief: segregatePreForkOutputs - set whether pre-fork outputs are to be segregated
    * param: segregate -
    */
    virtual void segregatePreForkOutputs(bool segregate) = 0;
    /**
    * brief: segregationHeight - set the height where segregation should occur
    * param: height -
    */
    virtual void segregationHeight(uint64_t height) = 0;
    /**
    * brief: keyReuseMitigation2 - set secondary key reuse mitigation
    * param: mitigation -
    */
    virtual void keyReuseMitigation2(bool mitigation) = 0;
    /**
    * brief: lockKeysFile -
    * return: true if succeeded
    */
    virtual bool lockKeysFile() = 0;
    /**
    * brief: unlockKeysFile -
    * return: true if succeeded
    */
    virtual bool unlockKeysFile() = 0;
    /**
    * brief: isKeysFileLocked -
    * return: true if keys file is locked
    */
    virtual bool isKeysFileLocked() = 0;
    /**
    * brief: getDeviceType -
    * return: device type
    */
    virtual Device getDeviceType() const = 0;
    // TODO : this is not really helpful
    /**
    * brief: coldKeyImageSync - cold-device protocol key image sync
    * param: spent -
    * param: unspent -
    * return:
    */
    virtual uint64_t coldKeyImageSync(uint64_t &spent, uint64_t &unspent) = 0;
    /**
    * brief: deviceShowAddress - show address on device display
    * param: accountIndex - major index
    * param: addressIndex - minor index
    * param: paymentId -
    */
    virtual void deviceShowAddress(uint32_t accountIndex, uint32_t addressIndex, const std::string &paymentId) = 0;
    /**
    * brief: reconnectDevice - attempt to reconnect to hardware device
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool reconnectDevice() = 0;
    /**
    * brief: getBytesReceived - network stats
    * return: bytes received
    */
    virtual uint64_t getBytesReceived() = 0;
    /**
    * brief: getBytesSent - network stats
    * return: bytes sent
    */
    virtual uint64_t getBytesSent() = 0;


    // Multisig
    /**
    * brief: multisig - get the current state of multisig wallet creation process
    * return: multisig state
    */
    virtual MultisigState multisig() const = 0;
    /**
    * brief: getMultisigInfo - get multisig first round kex message
    * return: serialized and signed multisig info
    * note: sets status error on fail
    */
    virtual std::string getMultisigInfo() const = 0;
    /**
    * brief: makeMultisig - switch wallet in multisig state. The one and only creation phase for N / N wallets
    * param: info         - vector of multisig infos from other participants obtained with getMulitisInfo call
    * param: threshold    - number of required signers to make valid transaction. Must be <= number of participants
    * return: empty string in case of N / N wallets, since no more key exchanges needed
    *         for N - 1 / N wallets returns base58 encoded extra multisig info
    * note: sets status error on fail
    */
    virtual std::string makeMultisig(const std::vector<std::string> &info, uint32_t threshold) = 0;
    /**
    * brief: exchangeMultisigKeys - provide additional key exchange round for arbitrary multisig schemes (like N-1/N, M/N)
    * param: info                 - base58 encoded key derivations returned by makeMultisig or exchangeMultisigKeys function call
    * param: force_update_use_with_caution - force multisig account to update even if not all signers contribute round messages
    * return: new info string if more rounds required, else an empty string if wallet creation is done
    * note: sets status error on fail
    */
    virtual std::string exchangeMultisigKeys(const std::vector<std::string> &info, const bool force_update_use_with_caution) = 0;
    /**
    * brief: exportMultisigImages - export transfers' key images
    * outparam: images            - hex encoded array of images
    * return: true if succeeded
    * note: sets status error on fail
    */
    virtual bool exportMultisigImages(std::string &images) = 0;
    /**
    * brief: importMultisigImages - imports other participants' multisig images
    * outparam: images            - array of hex encoded arrays of images obtained with exportMultisigImages
    * return: number of imported images
    * note: sets status error on fail
    */
    virtual size_t importMultisigImages(const std::vector<std::string> &images) = 0;
    /**
    * brief: hasMultisigPartialKeyImages - check if wallet needs to import multisig key images from other participants
    * return: true if there are partial key images
    * note: sets status error on fail
    */
    virtual bool hasMultisigPartialKeyImages() const = 0;
    /**
    * brief: restoreMultisigTransaction - create PendingTransaction from signData
    * param: signData - encrypted unsigned transaction. Obtained with PendingTransaction::multisigSignData
    * return: pending transaction if succeeded, else nullptr
    * note: sets status error on fail
    */
    virtual PendingTransaction * restoreMultisigTransaction(const std::string &signData) = 0;
    /**
    * brief: signMultisigParticipant - sign given message with the multisig public signer key
    * param: message - message to sign
    * return: signature if succeeded, else empty string
    * note: sets status error on fail
    */
    virtual std::string signMultisigParticipant(const std::string &message) const = 0;
    /**
    * brief: verifyMessageWithPublicKey - try to verify the message was signed with given public key
    * param: message   - message to verify
    * param: publicKey - public key used to sign the message
    * param: signature - the proof to verify
    * return: true if signature is valid, else and in case of error return false
    * note: sets status error on fail
    */
    virtual bool verifyMessageWithPublicKey(const std::string &message, const std::string &publicKey, const std::string &signature) const = 0;


    // Static
    // TODO : consider to add function to change unit or decimal point like `simple_wallet::set_unit()`
    /**
    * brief: displayAmount - convert from atomic units (piconero) to monero
    * param: amount - in atomic units
    * return: amount with decimal point
    */
    static std::string displayAmount(uint64_t amount);
    /**
    * brief: amountFromString - convert from monero (with decimal point) to atomic units (piconero)
    * param: amount - in monero (with decimal point)
    * return: amount in atomic units, 0 on error
    */
    static uint64_t amountFromString(const std::string &amount);
    /**
    * brief: amountFromDouble - convert from monero (with decimal point) to atomic units (piconero)
    * param: amount - in monero (with decimal point)
    * return: amount in atomic units, 0 on error
    */
    static uint64_t amountFromDouble(double amount);
    /**
    * brief: genPaymentId - generate a random payment id
    * return: payment id as 16 character hex string
    */
    static std::string genPaymentId();
    /**
    * brief: paymentIdValid - check if given payment id is valid
    * param: payment_id - either short (16 char hex) or long (64 char hex, DEPRECATED)
    * return: true if payment_id is valid
    */
    static bool paymentIdValid(const std::string &payment_id);
    /**
    * brief: addressValid - check if given address is valid
    * param: str     - address
    * param: nettype -
    * return: true if address is valid and belongs to given nettype
    */
    static bool addressValid(const std::string &str, NetworkType nettype);
    static bool addressValid(const std::string &str, bool testnet)          // deprecated
    {
        return addressValid(str, testnet ? TESTNET : MAINNET);
    }
    /**
    * brief: keyValid - check if given secret key belongs to given address address
    * param: secret_key_string -
    * param: address_string    -
    * param: isViewKey         - check address public view key match if true, else check spend key
    * param: nettype           -
    * outparam: error          - error message if error occurred
    * return: true if secret key
    */
    static bool keyValid(const std::string &secret_key_string, const std::string &address_string, bool isViewKey, NetworkType nettype, std::string &error);
    static bool keyValid(const std::string &secret_key_string, const std::string &address_string, bool isViewKey, bool testnet, std::string &error)     // deprecated
    {
        return keyValid(secret_key_string, address_string, isViewKey, testnet ? TESTNET : MAINNET, error);
    }
    /**
    * brief: paymentIdFromAddress - get payment id from address
    * param: str - address
    * param: nettype -
    * return: payment id if address contains one, else empty string
    */
    static std::string paymentIdFromAddress(const std::string &str, NetworkType nettype);
    static std::string paymentIdFromAddress(const std::string &str, bool testnet)       // deprecated
    {
        return paymentIdFromAddress(str, testnet ? TESTNET : MAINNET);
    }
    /**
    * brief: maximumAllowedAmount -
    * return: max numeric limit for uint64_t
    */
    static uint64_t maximumAllowedAmount();


    // Easylogger wrapper
    static void init(const char *argv0, const char *default_log_base_name) { init(argv0, default_log_base_name, "", true); }
    static void init(const char *argv0, const char *default_log_base_name, const std::string &log_path, bool console);
    static void debug(const std::string &category, const std::string &str);
    static void info(const std::string &category, const std::string &str);
    static void warning(const std::string &category, const std::string &str);
    static void error(const std::string &category, const std::string &str);
};

/**
 * @brief WalletManager - provides functions to manage wallets
 */
struct WalletManager
{

    /*!
     * \brief  Creates new wallet
     * \param  path           Name of wallet file
     * \param  password       Password of wallet file
     * \param  language       Language to be used to generate electrum seed mnemonic
     * \param  nettype        Network type
     * \param  kdf_rounds     Number of rounds for key derivation function
     * \return                Wallet instance (Wallet::status() needs to be called to check if created successfully)
     */
    virtual Wallet * createWallet(const std::string &path, const std::string &password, const std::string &language, NetworkType nettype, uint64_t kdf_rounds = 1) = 0;
    Wallet * createWallet(const std::string &path, const std::string &password, const std::string &language, bool testnet = false)      // deprecated
    {
        return createWallet(path, password, language, testnet ? TESTNET : MAINNET);
    }

    /*!
     * \brief  Opens existing wallet
     * \param  path           Name of wallet file
     * \param  password       Password of wallet file
     * \param  nettype        Network type
     * \param  kdf_rounds     Number of rounds for key derivation function
     * \param  listener       Wallet listener to set to the wallet after creation
     * \return                Wallet instance (Wallet::status() needs to be called to check if opened successfully)
     */
    virtual Wallet * openWallet(const std::string &path, const std::string &password, NetworkType nettype, uint64_t kdf_rounds = 1, WalletListener * listener = nullptr) = 0;
    Wallet * openWallet(const std::string &path, const std::string &password, bool testnet = false)     // deprecated
    {
        return openWallet(path, password, testnet ? TESTNET : MAINNET);
    }

    /*!
     * \brief  recovers existing wallet using mnemonic (electrum seed)
     * \param  path           Name of wallet file to be created
     * \param  password       Password of wallet file
     * \param  mnemonic       mnemonic (25 words electrum seed)
     * \param  nettype        Network type
     * \param  restoreHeight  restore from start height
     * \param  kdf_rounds     Number of rounds for key derivation function
     * \param  seed_offset    Seed offset passphrase (optional)
     * \return                Wallet instance (Wallet::status() needs to be called to check if recovered successfully)
     */
    virtual Wallet * recoveryWallet(const std::string &path, const std::string &password, const std::string &mnemonic,
                                    NetworkType nettype = MAINNET, uint64_t restoreHeight = 0, uint64_t kdf_rounds = 1,
                                    const std::string &seed_offset = {}) = 0;
    Wallet * recoveryWallet(const std::string &path, const std::string &password, const std::string &mnemonic,
                                    bool testnet = false, uint64_t restoreHeight = 0)           // deprecated
    {
        return recoveryWallet(path, password, mnemonic, testnet ? TESTNET : MAINNET, restoreHeight);
    }

    /*!
     * \deprecated this method creates a wallet WITHOUT a passphrase, use the alternate recoverWallet() method
     * \brief  recovers existing wallet using mnemonic (electrum seed)
     * \param  path           Name of wallet file to be created
     * \param  mnemonic       mnemonic (25 words electrum seed)
     * \param  nettype        Network type
     * \param  restoreHeight  restore from start height
     * \return                Wallet instance (Wallet::status() needs to be called to check if recovered successfully)
     */
    virtual Wallet * recoveryWallet(const std::string &path, const std::string &mnemonic, NetworkType nettype, uint64_t restoreHeight = 0) = 0;
    Wallet * recoveryWallet(const std::string &path, const std::string &mnemonic, bool testnet = false, uint64_t restoreHeight = 0)         // deprecated
    {
        return recoveryWallet(path, mnemonic, testnet ? TESTNET : MAINNET, restoreHeight);
    }

    /*!
     * \brief  recovers existing wallet using keys. Creates a view only wallet if spend key is omitted
     * \param  path           Name of wallet file to be created
     * \param  password       Password of wallet file
     * \param  language       language
     * \param  nettype        Network type
     * \param  restoreHeight  restore from start height
     * \param  addressString  public address
     * \param  viewKeyString  view key
     * \param  spendKeyString spend key (optional)
     * \param  kdf_rounds     Number of rounds for key derivation function
     * \return                Wallet instance (Wallet::status() needs to be called to check if recovered successfully)
     */
    virtual Wallet * createWalletFromKeys(const std::string &path,
                                                    const std::string &password,
                                                    const std::string &language,
                                                    NetworkType nettype,
                                                    uint64_t restoreHeight,
                                                    const std::string &addressString,
                                                    const std::string &viewKeyString,
                                                    const std::string &spendKeyString = "",
                                                    uint64_t kdf_rounds = 1) = 0;
    Wallet * createWalletFromKeys(const std::string &path,
                                  const std::string &password,
                                  const std::string &language,
                                  bool testnet,
                                  uint64_t restoreHeight,
                                  const std::string &addressString,
                                  const std::string &viewKeyString,
                                  const std::string &spendKeyString = "")       // deprecated
    {
        return createWalletFromKeys(path, password, language, testnet ? TESTNET : MAINNET, restoreHeight, addressString, viewKeyString, spendKeyString);
    }

   /*!
    * \deprecated this method creates a wallet WITHOUT a passphrase, use createWalletFromKeys(..., password, ...) instead
    * \brief  recovers existing wallet using keys. Creates a view only wallet if spend key is omitted
    * \param  path           Name of wallet file to be created
    * \param  language       language
    * \param  nettype        Network type
    * \param  restoreHeight  restore from start height
    * \param  addressString  public address
    * \param  viewKeyString  view key
    * \param  spendKeyString spend key (optional)
    * \return                Wallet instance (Wallet::status() needs to be called to check if recovered successfully)
    */
    virtual Wallet * createWalletFromKeys(const std::string &path, 
                                                    const std::string &language,
                                                    NetworkType nettype, 
                                                    uint64_t restoreHeight,
                                                    const std::string &addressString,
                                                    const std::string &viewKeyString,
                                                    const std::string &spendKeyString = "") = 0;
    Wallet * createWalletFromKeys(const std::string &path, 
                                  const std::string &language,
                                  bool testnet, 
                                  uint64_t restoreHeight,
                                  const std::string &addressString,
                                  const std::string &viewKeyString,
                                  const std::string &spendKeyString = "")           // deprecated
    {
        return createWalletFromKeys(path, language, testnet ? TESTNET : MAINNET, restoreHeight, addressString, viewKeyString, spendKeyString);
    }

    /*!
     * \brief  creates wallet using hardware device.
     * \param  path                 Name of wallet file to be created
     * \param  password             Password of wallet file
     * \param  nettype              Network type
     * \param  deviceName           Device name
     * \param  restoreHeight        restore from start height (0 sets to current height)
     * \param  subaddressLookahead  Size of subaddress lookahead (empty sets to some default low value)
     * \param  kdf_rounds           Number of rounds for key derivation function
     * \param  listener             Wallet listener to set to the wallet after creation
     * \return                      Wallet instance (Wallet::status() needs to be called to check if recovered successfully)
     */
    virtual Wallet * createWalletFromDevice(const std::string &path,
                                            const std::string &password,
                                            NetworkType nettype,
                                            const std::string &deviceName,
                                            uint64_t restoreHeight = 0,
                                            const std::string &subaddressLookahead = "",
                                            uint64_t kdf_rounds = 1,
                                            WalletListener * listener = nullptr) = 0;

    /*!
     * \brief Closes wallet. In case operation succeeded, wallet object deleted. in case operation failed, wallet object not deleted
     * \param wallet        previously opened / created wallet instance
     * \return              None
     */
    virtual bool closeWallet(Wallet *wallet, bool store = true) = 0;

    /*
     * ! checks if wallet with the given name already exists
     */

    /*!
     * @brief TODO: delme walletExists - check if the given filename is the wallet
     * @param path - filename
     * @return - true if wallet exists
     */
    virtual bool walletExists(const std::string &path) = 0;

    /*!
     * @brief verifyWalletPassword - check if the given filename is the wallet
     * @param keys_file_name - location of keys file
     * @param password - password to verify
     * @param no_spend_key - verify only view keys?
     * @param kdf_rounds - number of rounds for key derivation function
     * @return - true if password is correct
     *
     * @note
     * This function will fail when the wallet keys file is opened because the wallet program locks the keys file.
     * In this case, Wallet::unlockKeysFile() and Wallet::lockKeysFile() need to be called before and after the call to this function, respectively.
     */
    virtual bool verifyWalletPassword(const std::string &keys_file_name, const std::string &password, bool no_spend_key, uint64_t kdf_rounds = 1) const = 0;

    /*!
     * \brief determine the key storage for the specified wallet file
     * \param device_type     (OUT) wallet backend as enumerated in Wallet::Device
     * \param keys_file_name  Keys file to verify password for
     * \param password        Password to verify
     * \return                true if password correct, else false
     *
     * for verification only - determines key storage hardware
     *
     */
    virtual bool queryWalletDevice(Wallet::Device& device_type, const std::string &keys_file_name, const std::string &password, uint64_t kdf_rounds = 1) const = 0;

    /*!
     * \brief findWallets - searches for the wallet files by given path name recursively
     * \param path - starting point to search
     * \return - list of strings with found wallets (absolute paths);
     */
    virtual std::vector<std::string> findWallets(const std::string &path) = 0;

    // TODO : look into how this works in WalletManager
    /**
    * brief: errorString -
    * return: error string in case of error status
    */
    virtual std::string errorString() const = 0;

    //! set the daemon address (hostname and port)
    virtual void setDaemonAddress(const std::string &address) = 0;

    //! returns whether the daemon can be reached, and its version number
    virtual bool connected(uint32_t *version = NULL) = 0;

    //! returns current blockchain height
    virtual uint64_t blockchainHeight() = 0;

    //! returns current blockchain target height
    virtual uint64_t blockchainTargetHeight() = 0;

    //! returns current network difficulty
    virtual uint64_t networkDifficulty() = 0;

    //! returns current mining hash rate (0 if not mining)
    virtual double miningHashRate() = 0;

    //! returns current block target
    virtual uint64_t blockTarget() = 0;

    //! returns true iff mining
    virtual bool isMining() = 0;

    //! starts mining with the set number of threads
    virtual bool startMining(const std::string &address, uint32_t threads = 1, bool background_mining = false, bool ignore_battery = true) = 0;

    //! stops mining
    virtual bool stopMining() = 0;

    //! resolves an OpenAlias address to a monero address
    virtual std::string resolveOpenAlias(const std::string &address, bool &dnssec_valid) const = 0;

    //! checks for an update and returns version, hash and url
    static std::tuple<bool, std::string, std::string, std::string, std::string> checkUpdates(
        const std::string &software,
        std::string subdir,
        const char *buildtag = nullptr,
        const char *current_version = nullptr);

    //! sets proxy address, empty string to disable
    virtual bool setProxy(const std::string &address) = 0;
};


struct WalletManagerFactory
{
    // logging levels for underlying library
    enum LogLevel {
        LogLevel_Silent = -1,
        LogLevel_0 = 0,
        LogLevel_1 = 1,
        LogLevel_2 = 2,
        LogLevel_3 = 3,
        LogLevel_4 = 4,
        LogLevel_Min = LogLevel_Silent,
        LogLevel_Max = LogLevel_4
    };

    static WalletManager * getWalletManager();
    static void setLogLevel(int level);
    static void setLogCategories(const std::string &categories);
};


}
