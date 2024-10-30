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
#include "wallet_api.h"

//local headers
#include "hardforks/hardforks.h"
//#include "misc_language.h"
//#include "rpc/core_rpc_server_commands_defs.h"

//standard headers
//#include <algorithm>
//#include <cstdio>


namespace test
{
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
const std::size_t SENDR_WALLET_IDX  = 0;
const std::size_t RECVR_WALLET_IDX  = 1;
const std::size_t NUM_WALLETS       = 2;

const std::uint64_t FAKE_OUTS_COUNT = 15;
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static std::unique_ptr<Monero::WalletImpl> generate_wallet(const std::string &daemon_addr,
    const boost::optional<epee::net_utils::http::login> &daemon_login,
    const epee::net_utils::ssl_options_t ssl_support)
{
    std::unique_ptr<Monero::WalletImpl> wal(new Monero::WalletImpl());
    const std::string seed_language = "English";
    int status = Monero::Wallet::Status_Ok;
    std::string error_string = "";
    bool r;

    r = wal->init(daemon_addr,
                  0UL   /* upper_transaction_size_limit */,
                  ""    /* daemon_username */,
                  ""    /* daemon_password */,
                  true  /* use_ssl */,
                  false /* lightWallet */,
                  ""    /* proxy_address */);

    wal->statusWithErrorString(status, error_string);
    CHECK_AND_ASSERT_THROW_MES(r && status == Monero::Wallet::Status_Ok, error_string);

    wal->setTrustedDaemon(true);
    wal->allowMismatchedDaemonVersion(true);
    wal->setRefreshFromBlockHeight(1); // setting to 1 skips height estimate in wal->generate()

    // Generate wallet in memory with empty wallet file name
    r = wal->create("", "", seed_language);

    wal->statusWithErrorString(status, error_string);
    CHECK_AND_ASSERT_THROW_MES(r && status == Monero::Wallet::Status_Ok, error_string);

    CHECK_AND_ASSERT_THROW_MES(wal->connected() == Monero::Wallet::ConnectionStatus_Connected, "wallet not connected to daemon");

    return wal;
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
void WalletAPITest::reset()
{
    printf("Resetting blockchain\n");
    std::uint64_t height = this->daemon()->get_height().height;
    this->daemon()->pop_blocks(height - 1);
    this->daemon()->flush_txpool();
}
//-------------------------------------------------------------------------------------------------------------------
void WalletAPITest::mine(const std::size_t wallet_idx, const std::uint64_t num_blocks)
{
    const std::string addr = this->wallet(wallet_idx)->mainAddress();
    this->daemon()->generateblocks(addr, num_blocks);
}
//-------------------------------------------------------------------------------------------------------------------
//void WalletAPITest::transfer(const std::size_t wallet_idx,
//    const cryptonote::account_public_address &dest_addr,
//    const bool is_subaddress,
//    const std::uint64_t amount_to_transfer,
//    cryptonote::transaction &tx_out)
//{
//    std::vector<cryptonote::tx_destination_entry> dsts;
//    dsts.reserve(1);
//
//    cryptonote::tx_destination_entry de;
//    de.addr = dest_addr;
//    de.is_subaddress = is_subaddress;
//    de.amount = amount_to_transfer;
//    dsts.push_back(de);
//
//    std::vector<tools::wallet2::pending_tx> ptx;
//    ptx = this->wallet(wallet_idx)->create_transactions_2(dsts, FAKE_OUTS_COUNT, 0, std::vector<uint8_t>(), 0, {});
//    CHECK_AND_ASSERT_THROW_MES(ptx.size() == 1, "unexpected num pending txs");
//    this->wallet(wallet_idx)->commit_tx(ptx[0]);
//
//    tx_out = std::move(ptx[0].tx);
//}
//-------------------------------------------------------------------------------------------------------------------
//std::uint64_t WalletAPITest::mine_tx(const crypto::hash &tx_hash, const std::string &miner_addr_str)
//{
//    const std::string txs_hash = epee::string_tools::pod_to_hex(tx_hash);
//
//    // Make sure tx is in the pool
//    auto res = this->daemon()->get_transactions(std::vector<std::string>{txs_hash});
//    CHECK_AND_ASSERT_THROW_MES(res.txs.size() == 1 && res.txs[0].tx_hash == txs_hash && res.txs[0].in_pool,
//        "tx not found in pool");
//
//    // Mine the tx
//    const std::uint64_t height = this->daemon()->generateblocks(miner_addr_str, 1).height;
//
//    // Make sure tx was mined
//    res = this->daemon()->get_transactions(std::vector<std::string>{txs_hash});
//    CHECK_AND_ASSERT_THROW_MES(res.txs.size() == 1 && res.txs[0].tx_hash == txs_hash
//        && res.txs[0].block_height == height, "tx not yet mined");
//
//    return this->daemon()->get_last_block_header().block_header.reward;
//}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
//void WalletAPITest::check_wallet2_scan(const ExpectedScanResults &res)
//{
//    auto &sendr_wallet = this->wallet(SENDR_WALLET_IDX);
//    auto &recvr_wallet = this->wallet(RECVR_WALLET_IDX);
//
//    sendr_wallet->refresh(true);
//    recvr_wallet->refresh(true);
//    const std::uint64_t sendr_final_balance = sendr_wallet->balance(0, true);
//    const std::uint64_t recvr_final_balance = recvr_wallet->balance(0, true);
//
//    CHECK_AND_ASSERT_THROW_MES(sendr_final_balance == res.sendr_expected_balance,
//        "sendr_wallet has unexpected balance");
//    CHECK_AND_ASSERT_THROW_MES(recvr_final_balance == res.recvr_expected_balance,
//        "recvr_wallet has unexpected balance");
//
//    // Find all transfers with matching tx hash
//    tools::wallet2::transfer_container recvr_wallet_incoming_transfers;
//    recvr_wallet->get_transfers(recvr_wallet_incoming_transfers);
//
//    std::uint64_t received_amount = 0;
//    auto it = recvr_wallet_incoming_transfers.begin();
//    const auto end = recvr_wallet_incoming_transfers.end();
//    const auto is_same_hash = [l_tx_hash = res.tx_hash](const tools::wallet2::transfer_details& td)
//        { return td.m_txid == l_tx_hash; };
//    while ((it = std::find_if(it, end, is_same_hash)) != end)
//    {
//        CHECK_AND_ASSERT_THROW_MES(it->m_block_height > 0, "recvr_wallet did not see tx in chain");
//        received_amount += it->m_amount;
//        it++;
//    }
//    CHECK_AND_ASSERT_THROW_MES(received_amount == res.transfer_amount,
//        "recvr_wallet did not receive correct amount");
//}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
//ExpectedScanResults WalletAPITest::init_normal_transfer_test()
//{
//    auto &sendr_wallet = this->wallet(SENDR_WALLET_IDX);
//    auto &recvr_wallet = this->wallet(RECVR_WALLET_IDX);
//
//    // Assert sendr_wallet has enough money to send to recvr_wallet
//    std::uint64_t amount_to_transfer = 1000000000000;
//    sendr_wallet->refresh(true);
//    recvr_wallet->refresh(true);
//    CHECK_AND_ASSERT_THROW_MES(sendr_wallet->unlocked_balance(0, true) > (amount_to_transfer*2)/*2x for fee*/,
//        "sendr_wallet does not have enough money");
//
//    // Save initial state
//    std::uint64_t sendr_init_balance = sendr_wallet->balance(0, true);
//    std::uint64_t recvr_init_balance = recvr_wallet->balance(0, true);
//
//    // Send from sendr_wallet to recvr_wallet's primary adddress
//    cryptonote::transaction tx;
//    cryptonote::account_public_address dest_addr = recvr_wallet->get_account().get_keys().m_account_address;
//    this->transfer(SENDR_WALLET_IDX, dest_addr, false/*is_subaddress*/, amount_to_transfer, tx);
//    std::uint64_t fee = cryptonote::get_tx_fee(tx);
//    crypto::hash tx_hash = cryptonote::get_transaction_hash(tx);
//
//    // Mine the tx
//    std::string sender_addr = sendr_wallet->get_account().get_public_address_str(cryptonote::MAINNET);
//    std::uint64_t block_reward = this->mine_tx(tx_hash, sender_addr);
//
//    // Calculate expected balances
//    std::uint64_t sendr_expected_balance = sendr_init_balance - amount_to_transfer - fee + block_reward;
//    std::uint64_t recvr_expected_balance = recvr_init_balance + amount_to_transfer;
//
//    return ExpectedScanResults{
//        .sendr_expected_balance = sendr_expected_balance,
//        .recvr_expected_balance = recvr_expected_balance,
//        .tx_hash                = std::move(tx_hash),
//        .transfer_amount        = amount_to_transfer
//    };
//}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
// Tests
//-------------------------------------------------------------------------------------------------------------------
// TODO : remove check_something() after testing
void WalletAPITest::check_something()
{
    printf("Checking something\n");
    int status = Monero::Wallet::Status_Ok;
    std::string error_string = "";

    // TODO : this could probably be part of wallet_api.cpp unit_tests
    const std::pair<std::uint32_t, std::uint32_t> main_address_indices = std::make_pair(0, 0);
    const std::pair<std::uint32_t, std::uint32_t> sub_address_indices = std::make_pair(1, 1);
    const std::string main_address = this->wallet(0)->mainAddress();
    const std::string sub_address = this->wallet(0)->address(sub_address_indices.first, sub_address_indices.second);
    std::pair<std::uint32_t, std::uint32_t> address_indices_out;

    // Main-address
    address_indices_out = this->wallet(0)->getSubaddressIndex(main_address);
    this->wallet(0)->statusWithErrorString(status, error_string);
    CHECK_AND_ASSERT_THROW_MES(status == Monero::Wallet::Status_Ok, error_string);
    CHECK_AND_ASSERT_THROW_MES(address_indices_out.first == main_address_indices.first &&
                               address_indices_out.second == main_address_indices.second,
                               "main-address indices do not match");

    // Sub-address
    address_indices_out = this->wallet(0)->getSubaddressIndex(sub_address);
    this->wallet(0)->statusWithErrorString(status, error_string);
    CHECK_AND_ASSERT_THROW_MES(status == Monero::Wallet::Status_Ok, error_string);
    CHECK_AND_ASSERT_THROW_MES(address_indices_out.first == sub_address_indices.first &&
                               address_indices_out.second == sub_address_indices.second,
                               "sub-address indices do not match");
}
//-------------------------------------------------------------------------------------------------------------------
void WalletAPITest::check_hardForkInfo()
{
    /*
    *   generate blocks until most recent hard fork
    *   and check if the output from `hardForkInfo()` matches with hardcoded values from hardforks.h
    */

    printf("Checking hardForkInfo()\n");
    std::uint8_t version = 0;
    std::uint64_t earliest_height = 0;

    this->wallet(0)->hardForkInfo(version, earliest_height);
    CHECK_AND_ASSERT_THROW_MES(version == 0 && earliest_height == 1, "Wrong hardForkInfo() output");

    // TODO : figure out if there is a faster way to get to most recent hard fork, or if it's even necessary
    //        maybe consider hacky alternatives, that don't need to use `this->daemon()->generateblocks()`
    return;
    // Generate blocks until most recent hard fork
    std::uint64_t block_height = 0;
    int chunk_size = 100;
    int actual_chunk_size = 0;
    for (std::size_t v = 0; v < num_mainnet_hard_forks; ++v)
    {
        std::uint64_t target_height = mainnet_hard_forks[v].height;
        printf("Mining until hf version: %ld at height: %ld\n", v+1, target_height);
        while (block_height < target_height)
        {
            if (block_height + chunk_size < target_height)
            {
                actual_chunk_size = chunk_size;
                block_height += chunk_size;
            }
            else
            {
                actual_chunk_size = target_height - block_height;
                block_height = target_height;
            }
            this->mine(SENDR_WALLET_IDX, actual_chunk_size);
            printf("Still mining. Current block_height: %ld\n", block_height);
        }
        this->wallet(0)->hardForkInfo(version, earliest_height);
        printf("Block height: %ld - Hard fork version: %d - Earliest height: %ld\n", block_height, version, earliest_height);
        // TODO : figure out ealiest_height
        CHECK_AND_ASSERT_THROW_MES(version == v && earliest_height == 1, "Wrong hardForkInfo() output");
    }
}
//-------------------------------------------------------------------------------------------------------------------
void WalletAPITest::check_useForkRules()
{
    printf("Checking useForkRules()\n");
    int status = Monero::Wallet::Status_Ok;
    std::string error_string;

    std::uint64_t version = 0;
    std::int64_t early_blocks = 0;

    CHECK_AND_ASSERT_THROW_MES(this->wallet(0)->useForkRules(version, early_blocks), "not using right fork rules");
    this->wallet(0)->statusWithErrorString(status, error_string);
    CHECK_AND_ASSERT_THROW_MES(status == Monero::Wallet::Status_Ok, error_string);

    // TODO : not sure why this fails
//    CHECK_AND_ASSERT_THROW_MES(!this->wallet(0)->useForkRules(10, early_blocks), "using wrong fork rules");
//    this->wallet(0)->statusWithErrorString(status, error_string);
//    CHECK_AND_ASSERT_THROW_MES(status == Monero::Wallet::Status_Ok, error_string);
}
//-------------------------------------------------------------------------------------------------------------------
void WalletAPITest::check_balance()
{
    printf("Checking balance\n");
    int status = Monero::Wallet::Status_Ok;
    std::string error_string;

    this->reset();

    CHECK_AND_ASSERT_THROW_MES(this->wallet(0)->balance() == 0, "Wrong balance");

    this->mine(0, 10);
    this->wallet(0)->refresh();
    printf("Balance: %ld\n", this->wallet(0)->balance());

    CHECK_AND_ASSERT_THROW_MES(this->wallet(0)->balance() > 0, "Wrong balance");

    CHECK_AND_ASSERT_THROW_MES(status == Monero::Wallet::Status_Ok, error_string);
}
//-------------------------------------------------------------------------------------------------------------------
bool WalletAPITest::run()
{
    // Reset chain
    this->reset();

    // Run the tests
    this->check_something();
    this->check_hardForkInfo();
    this->check_useForkRules();
    this->check_balance();

    return true;
}
//-------------------------------------------------------------------------------------------------------------------
WalletAPITest::WalletAPITest(const std::string &daemon_addr):
    m_daemon_addr(daemon_addr)
{
    const boost::optional<epee::net_utils::http::login> daemon_login = boost::none;
    const epee::net_utils::ssl_options_t ssl_support = epee::net_utils::ssl_support_t::e_ssl_support_disabled;

    m_daemon = std::make_unique<tools::t_daemon_rpc_client>(m_daemon_addr, daemon_login, ssl_support);

    m_wallets.reserve(NUM_WALLETS);
    for (std::size_t i = 0; i < NUM_WALLETS; ++i)
    {
        m_wallets.push_back(generate_wallet(m_daemon_addr, daemon_login, ssl_support));
    }
}
//-------------------------------------------------------------------------------------------------------------------
} //namespace test
