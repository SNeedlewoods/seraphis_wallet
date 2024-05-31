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
// TODO : Not sure about command line options for the API in general, but both simplewallet and wallet_rpc use these, so I think we need to support them somehow
TEST(wallet_api, command_line_options)
{
    boost::program_options::variables_map vm;
    boost::program_options::options_description desc_params(wallet_args::tr("Wallet API options"));
    int argc = 0;
    const char* argv[6];
    argv[0] = NULL;

    // init wallet options
    WalletImpl::initOptions(desc_params);
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc_params), vm);

    // check defaults
    ASSERT_FALSE(WalletImpl::hasTestnetOption(vm));
    ASSERT_FALSE(WalletImpl::hasStagenetOption(vm));
    ASSERT_TRUE(WalletImpl::deviceNameOption(vm) == "");

    // add/change options
    const command_line::arg_descriptor<std::string, true> arg_testnet = {"testnet", "testnet"};
    const command_line::arg_descriptor<std::string, true> arg_stagenet = {"stagenet", "stagenet"};
    const command_line::arg_descriptor<std::string, true> arg_device_name = {"hw-device", "hw-device <device-name>"};
    command_line::add_arg(desc_params, arg_testnet);
    command_line::add_arg(desc_params, arg_stagenet);
    command_line::add_arg(desc_params, arg_device_name);

    argc = 5;
    argv[0] = "wallet-api";
    argv[1] = "--testnet";
    argv[2] = "--stagenet";
    argv[3] = "--hw-device";
    argv[4] = "default";
    argv[5] = NULL;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc_params), vm);

    // check added/changed options
    ASSERT_TRUE(WalletImpl::hasTestnetOption(vm));
    ASSERT_TRUE(WalletImpl::hasStagenetOption(vm));
    ASSERT_TRUE(WalletImpl::deviceNameOption(vm) == "default");
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
