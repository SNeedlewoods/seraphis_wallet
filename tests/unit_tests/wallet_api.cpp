
// Copyright (c) 2024, The Monero Project
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

#include <iostream>

#include <memory>
#include "boost/filesystem.hpp"
#include "wallet/api/wallet2_api.h"
#include "wallet/api/wallet.h"
#include "wallet/api/wallet_manager.h"
#include "gtest/gtest.h"
#include <memory>

// TODO :
//  - This does not really fit into unit_tests, because it takes additional steps to setup (running a private testnet)
/*
   Setup
   1. Run testnet daemon
   monerod
   2. Run 
*/

//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, create_wallet)
{
    // set wallet creation parameters
    std::string wallet_path = "test_create_wallet";
    std::string wallet_password = "wallet password";
    std::string seed_language = "English";

    boost::filesystem::remove(wallet_path);
    boost::filesystem::remove(wallet_path + ".keys");

    // create mainnet wallet
    std::unique_ptr<Monero::WalletImpl> wallet = std::make_unique<Monero::WalletImpl>();
    EXPECT_TRUE(wallet->create(wallet_path, wallet_password, seed_language));

    // clean-up
    // TODO : uncomment if done testing created file with cli
//    boost::filesystem::remove(wallet_path);
//    boost::filesystem::remove(wallet_path + ".keys");
}
//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, restore_wallet)
{
    // set wallet creation parameters
    std::string wallet_path = "test_restore_wallet";
    std::string wallet_password = "wallet password";
    std::string seed_language = "English";
    // TODO : Consider changing this to most recent testnet hard fork size (currently `1983520`, src: https://github.com/monero-project/monero/blob/master/src/hardforks/hardforks.cpp#L102C9-L102C16)
    std::uint64_t restore_height = 0;
    std::string address_str = "9wBbVrv9XbnMyK5GAtsiT3S15SAZoyriE62XYmtf5b3dcCZuERotQsoVxvvLbTvqYPR5UTt1epfqv8ckbYW2usN5GYpeReK";
    std::string view_key_str = "";
    std::string spend_key_str = "0000000000000000000000000000000000000000000000000000000000000002";

    boost::filesystem::remove(wallet_path);
    boost::filesystem::remove(wallet_path + ".keys");

    Monero::WalletManager *wallet_manager = Monero::WalletManagerFactory::getWalletManager();
//    std::unique_ptr<Monero::WalletImpl> wallet = std::make_unique<Monero::WalletImpl>(
//                                            wallet_manager->createWalletFromKeys(
//                                                wallet_path,
//                                                wallet_password,
//                                                seed_language,
//                                                Monero::NetworkType::TESTNET,
//                                                restore_height,
//                                                address_str,
//                                                view_key_str,
//                                                spend_key_str
//                                            )
//                                        );
    Monero::Wallet *wallet = wallet_manager->createWalletFromKeys(
                                                    wallet_path,
                                                    wallet_password,
                                                    seed_language,
                                                    Monero::NetworkType::TESTNET,
                                                    restore_height,
                                                    address_str,
                                                    view_key_str,
                                                    spend_key_str
                                                );
    wallet_manager->closeWallet(wallet);


    // clean-up
    // TODO : uncomment if done testing created file with cli
//    boost::filesystem::remove(wallet_path);
//    boost::filesystem::remove(wallet_path + ".keys");
}
//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, subaddress_index)
{

}
//-------------------------------------------------------------------------------------------------------------------

// TODO : add tests for all new wallet api methods 
/*
    std::string getMultisigSeed(const std::string &seed_offset) const override;
    std::pair<std::uint32_t, std::uint32_t> getSubaddressIndex(const std::string &address) const override;
    void freeze(std::size_t idx) override;
    void freeze(const std::string &key_image) override;
    void thaw(std::size_t idx) override;
    void thaw(const std::string &key_image) override;
    bool isFrozen(std::size_t idx) const override;
    bool isFrozen(const std::string &key_image) const override;
    void createOneOffSubaddress(std::uint32_t account_index, std::uint32_t address_index) override;
    WalletState getWalletState() const override;
    void rewriteWalletFile(const std::string &wallet_name, const std::string &password) override;
    void writeWatchOnlyWallet(const std::string &password, std::string &new_keys_file_name) override;
    void updatePoolState(std::vector<std::tuple<cryptonote::transaction, std::string, bool>> &process_txs, bool refreshed = false, bool try_incremental = false) override;
    void processPoolState(const std::vector<std::tuple<cryptonote::transaction, std::string, bool>> &txs) override;
    void getEnoteDetails(std::vector<std::shared_ptr<EnoteDetails>> &enote_details) const override;
    std::string convertMultisigTxToStr(const PendingTransaction &multisig_ptx) const override;
    bool saveMultisigTx(const PendingTransaction &multisig_ptx, const std::string &filename) const override;
    std::string convertTxToStr(const PendingTransaction &ptxs) const override;
    bool parseUnsignedTxFromStr(const std::string &unsigned_tx_str, UnsignedTransaction &exported_txs) const override;
    bool parseTxFromStr(const std::string &signed_tx_str, PendingTransaction &ptx) const override;
    void insertColdKeyImages(PendingTransaction &ptx) override;
    bool parseMultisigTxFromStr(const std::string &multisig_tx_str, PendingTransaction &exported_txs) const override;
    std::uint64_t getFeeMultiplier(std::uint32_t priority, int fee_algorithm) const override;
    std::uint64_t getBaseFee() const override;
    std::uint32_t adjustPriority(std::uint32_t priority) override;
    void coldTxAuxImport(const PendingTransaction &ptx, const std::vector<std::string> &tx_device_aux) const override;
    void coldSignTx(const PendingTransaction &ptx_in, PendingTransaction &exported_txs_out, std::vector<cryptonote::address_parse_info> &dsts_info) const override;
    void discardUnmixableEnotes() override;
    void setTxKey(const std::string &txid, const std::string &tx_key, const std::vector<std::string> &additional_tx_keys, const std::string &single_destination_subaddress) override;
    const std::pair<std::map<std::string, std::string>, std::vector<std::string>>& getAccountTags() const override;
    void setAccountTag(const std::set<uint32_t> &account_indices, const std::string &tag) override;
    void setAccountTagDescription(const std::string &tag, const std::string &description) override;
    std::string exportEnotesToStr(bool all = false, std::uint32_t start = 0, std::uint32_t count = 0xffffffff) const override;
    std::size_t importEnotesFromStr(const std::string &enotes_str) override;
    std::uint64_t getBlockchainHeightByDate(std::uint16_t year, std::uint8_t month, std::uint8_t day) const override;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> estimateBacklog(const std::vector<std::pair<double, double>> &fee_levels) const override;
    bool saveToFile(const std::string &path_to_file, const std::string &binary, bool is_printable = false) const override;
    bool loadFromFile(const std::string &path_to_file, std::string &target_str, std::size_t max_size = 1000000000) const override;
    std::uint64_t hashEnotes(std::uint64_t enote_idx, std::string &hash) const override;
    void finishRescanBcKeepKeyImages(std::uint64_t enote_idx, const std::string &hash) override;
    std::vector<std::tuple<std::string, std::uint16_t, std::uint64_t>> getPublicNodes(bool white_only = true) const override;
    std::pair<std::size_t, std::uint64_t> estimateTxSizeAndWeight(bool use_rct, int n_inputs, int ring_size, int n_outputs, std::size_t extra_size) const override;
    std::uint64_t importKeyImages(const std::vector<std::pair<std::string, std::string>> &signed_key_images, std::size_t offset, std::uint64_t &spent, std::uint64_t &unspent, bool check_spent = true) override;
    bool importKeyImages(std::vector<std::string> key_images, std::size_t offset = 0, std::unordered_set<std::size_t> selected_enotes_indices = {}) override;
*/
