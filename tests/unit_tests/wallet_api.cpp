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

#include "common/scoped_message_writer.h"
#include "cryptonote_core/cryptonote_core.h"
#include "wallet/api/wallet.h"
#include "wallet/wallet_args.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>

#include "unit_tests_utils.h"
#include "gtest/gtest.h"

using namespace Monero;

//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, convert_amounts)
{
    // Amount
    std::uint64_t amount_1_pico =             1; // 1 piconero
    std::uint64_t amount_1_xmr  = 1000000000000; // 1 XMR
    std::uint64_t amount_max    = Wallet::maximumAllowedAmount(); // max amount

    std::string amount_1_pico_s =        "0.000000000001"; // 1 piconero
    std::string amount_1_xmr_s  =        "1.000000000000"; // 1 XMR
    std::string amount_max_s    = "18446744.073709551615"; // max amount

    double amount_1_pico_d      =        0.000000000001; // 1 piconero
    double amount_1_xmr_d       =        1.000000000000; // 1 XMR
    // TODO : figure out max amount for double
    double amount_max_d         = 18446744.073709551615; // max amount

    // uint64_t to string
    ASSERT_TRUE(Wallet::displayAmount(amount_1_pico) == amount_1_pico_s);
    ASSERT_TRUE(Wallet::displayAmount(amount_1_xmr)  == amount_1_xmr_s);
    ASSERT_TRUE(Wallet::displayAmount(amount_max)    == amount_max_s);

    // string to uint64_t
    ASSERT_TRUE(Wallet::amountFromString(amount_1_pico_s) == amount_1_pico);
    ASSERT_TRUE(Wallet::amountFromString(amount_1_xmr_s)  == amount_1_xmr);
    ASSERT_TRUE(Wallet::amountFromString(amount_max_s)    == amount_max);

    // double to uint64_t
    ASSERT_TRUE(Wallet::amountFromDouble(amount_1_pico_d) == amount_1_pico);
    ASSERT_TRUE(Wallet::amountFromDouble(amount_1_xmr_d)  == amount_1_xmr);
    // TODO : investigate, returned value is 18446744073709551245
    //                     expected value is 18446744073709551615
//    ASSERT_TRUE(Wallet::amountFromDouble(amount_max_d)    == amount_max);
}
//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, generate_and_validate_payment_id)
{
    std::string payment_id_valid_short = Wallet::genPaymentId();
    std::string payment_id_valid_long  = Wallet::genPaymentId() + Wallet::genPaymentId() + Wallet::genPaymentId() + Wallet::genPaymentId();

    std::string payment_id_zeros = "0000000000000000";

    std::string payment_id_invalid_length    = payment_id_valid_short + "0";
    std::string payment_id_invalid_character = payment_id_valid_short.substr(0, 15) + "g";

    ASSERT_TRUE(Wallet::paymentIdValid(payment_id_valid_short));
    ASSERT_TRUE(Wallet::paymentIdValid(payment_id_valid_long));

    // TODO : is this indeed a valid payment id (all zeros) or a bug in paymentIdValid?
    ASSERT_TRUE(Wallet::paymentIdValid(payment_id_zeros));

    ASSERT_FALSE(Wallet::paymentIdValid(payment_id_invalid_length));
    ASSERT_FALSE(Wallet::paymentIdValid(payment_id_invalid_character));
}
//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, validate_key_and_address)
{
    // generate keys and address
    NetworkType mainnet = NetworkType::MAINNET;
    crypto::secret_key secret_spend_key;
    crypto::secret_key secret_view_key;
    crypto::public_key pub_k;
    cryptonote::account_base account{};

    crypto::generate_keys(pub_k, secret_spend_key, secret_spend_key, false /* recover */);
    account.generate(secret_spend_key);

    std::string mainaddress = account.get_public_address_str(static_cast<cryptonote::network_type>(mainnet));
    std::string secret_spend_key_str = epee::string_tools::pod_to_hex(account.get_keys().m_spend_secret_key);
    std::string secret_view_key_str = epee::string_tools::pod_to_hex(account.get_keys().m_view_secret_key);
    std::string error = "";

    ASSERT_TRUE(Wallet::addressValid(mainaddress, mainnet));
    ASSERT_TRUE(Wallet::keyValid(secret_spend_key_str, mainaddress, false /* isViewKey */, mainnet, error));
    ASSERT_TRUE(Wallet::keyValid(secret_view_key_str,  mainaddress, true  /* isViewKey */, mainnet, error));

    // get payment id from integrated address
    std::string payment_id_hex = Wallet::genPaymentId();
    crypto::hash8 payment_id;
    std::string integrated_address;

    epee::string_tools::hex_to_pod(payment_id_hex, payment_id);
    integrated_address = account.get_public_integrated_address_str(payment_id, static_cast<cryptonote::network_type>(mainnet));

    ASSERT_TRUE(Wallet::paymentIdFromAddress(integrated_address, mainnet) == payment_id_hex);
}
//-------------------------------------------------------------------------------------------------------------------
// TODO : Not sure about command line options for the API in general, but both simplewallet and wallet_rpc use these, so I think we need to support them somehow
TEST(wallet_api, command_line_options)
{
    /*
       Test static `WalletImpl` functions for command line options
    */
    boost::program_options::variables_map vm;
    boost::program_options::options_description desc_params(wallet_args::tr("Wallet API options"));
    int argc = 0;
    const char* argv[8];
    argv[0] = NULL;

    // init wallet options
    WalletImpl::initOptions(desc_params);
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc_params), vm);

    // check defaults
    ASSERT_FALSE(WalletImpl::hasTestnetOption(vm));
    ASSERT_FALSE(WalletImpl::hasStagenetOption(vm));
    ASSERT_TRUE(WalletImpl::deviceNameOption(vm) == "");
    ASSERT_TRUE(WalletImpl::deviceDerivationPathOption(vm) == "");

    // add/change options
    const command_line::arg_descriptor<std::string, true> arg_testnet = {"testnet", "testnet"};
    const command_line::arg_descriptor<std::string, true> arg_stagenet = {"stagenet", "stagenet"};
    const command_line::arg_descriptor<std::string, true> arg_device_name = {"hw-device", "hw-device <device-name>"};
    const command_line::arg_descriptor<std::string, true> arg_device_derivation_path = {"hw-device-deriv-path", "hw-device-deriv-path <path>"};
    command_line::add_arg(desc_params, arg_testnet);
    command_line::add_arg(desc_params, arg_stagenet);
    command_line::add_arg(desc_params, arg_device_name);
    command_line::add_arg(desc_params, arg_device_derivation_path);

    std::string device_name = "default";
    std::string device_derivation_path = "testpath";
    argc = 7;
    argv[0] = "wallet-api";
    argv[1] = "--testnet";
    argv[2] = "--stagenet";
    argv[3] = "--hw-device";
    argv[4] = device_name.c_str();
    argv[5] = "--hw-device-deriv-path";
    argv[6] = device_derivation_path.c_str();
    argv[7] = NULL;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc_params), vm);

    // check added/changed options
    ASSERT_TRUE(WalletImpl::hasTestnetOption(vm));
    ASSERT_TRUE(WalletImpl::hasStagenetOption(vm));
    ASSERT_TRUE(WalletImpl::deviceNameOption(vm) == device_name);
    ASSERT_TRUE(WalletImpl::deviceDerivationPathOption(vm) == device_derivation_path);
}
//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, create_wallet)
{
    /*
       create and store a wallet file
    */

    // set wallet creation parameters
    const boost::filesystem::path wallet_file = unit_test::data_dir / "wallet_api_create";
    const std::string wallet_password = "password";
    const std::string seed_language = "English";

    // clean-up, create() checks if file exists
    boost::filesystem::remove(wallet_file.string());
    boost::filesystem::remove(wallet_file.string() + ".keys");

    // create mainnet wallet
    WalletImpl *wallet = new WalletImpl();
    EXPECT_TRUE(wallet->create(wallet_file.string(), wallet_password, seed_language));
}
//-------------------------------------------------------------------------------------------------------------------
TEST(wallet_api, make_new_wallet)
{
    /*
       make new wallet object from command line arguments
    */

    // Initialize vm
//    boost::optional<boost::program_options::variables_map> vm;
//    bool should_terminate = false;
//    size_t argc = 0;
//    char* argv[0];
//    boost::program_options::options_description desc_params(wallet_args::tr("Wallet options"));
//    boost::program_options::positional_options_description positional_options;
//    const command_line::arg_descriptor< std::vector<std::string> > arg_command = {"command", ""};
//
//    positional_options.add(arg_command.name, -1);
//    tools::wallet2::init_options(desc_params);
//
//    std::tie(vm, should_terminate) = wallet_args::main(
//            argc, argv,
//            "",
//            tr(""),
//            desc_params,
//            positional_options,
//            [](const std::string &s, bool emphasis){ tools::scoped_message_writer(emphasis ? epee::console_color_white : epee::console_color_default, true) << s; },
//            "monero-wallet-api.log"
//            );
//
//    // Make wallet
//    std::pair<std::unique_ptr<tools::wallet2>, tools::password_container> rc;
//
//    try{ rc = WalletImpl::makeNew(*vm, false /* unattended */, nullptr /* password_prompter */); }
//    catch(const std::exception &e) { std::cout << tr("Error creating wallet: ") << e.what() << "\n"; return; }
//
//    std::cout << rc.first->get_device_type() << "\n";
}
