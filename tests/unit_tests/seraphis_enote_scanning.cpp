// Copyright (c) 2021, The Monero Project
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

#include "crypto/crypto.h"
#include "misc_language.h"
#include "ringct/rctOps.h"
#include "ringct/rctTypes.h"
#include "seraphis/jamtis_address_tag_utils.h"
#include "seraphis/jamtis_address_utils.h"
#include "seraphis/jamtis_core_utils.h"
#include "seraphis/jamtis_destination.h"
#include "seraphis/jamtis_enote_utils.h"
#include "seraphis/jamtis_payment_proposal.h"
#include "seraphis/jamtis_support_types.h"
#include "seraphis/mock_ledger_context.h"
#include "seraphis/sp_composition_proof.h"
#include "seraphis/sp_core_enote_utils.h"
#include "seraphis/sp_core_types.h"
#include "seraphis/sp_crypto_utils.h"
#include "seraphis/tx_base.h"
#include "seraphis/tx_binned_reference_set.h"
#include "seraphis/tx_builder_types.h"
#include "seraphis/tx_builders_inputs.h"
#include "seraphis/tx_builders_mixed.h"
#include "seraphis/tx_builders_outputs.h"
#include "seraphis/tx_component_types.h"
#include "seraphis/tx_discretized_fee.h"
#include "seraphis/tx_enote_finding_context_mocks.h"
#include "seraphis/tx_enote_record_types.h"
#include "seraphis/tx_enote_record_utils.h"
#include "seraphis/tx_enote_scanning.h"
#include "seraphis/tx_enote_scanning_context_simple.h"
#include "seraphis/tx_enote_store_mocks.h"
#include "seraphis/tx_enote_store_updater_mocks.h"
#include "seraphis/tx_extra.h"
#include "seraphis/tx_fee_calculator_mocks.h"
#include "seraphis/tx_fee_calculator_squashed_v1.h"
#include "seraphis/tx_input_selection.h"
#include "seraphis/tx_input_selection_output_context_v1.h"
#include "seraphis/tx_input_selector_mocks.h"
#include "seraphis/tx_misc_utils.h"
#include "seraphis/tx_validation_context_mock.h"
#include "seraphis/txtype_squashed_v1.h"

#include "boost/multiprecision/cpp_int.hpp"
#include "gtest/gtest.h"


class Invocable
{
public:
    virtual ~Invocable() = default;
    Invocable& operator=(Invocable&&) = delete;
    virtual void invoke() = 0;
};

class DummyInvocable final : public Invocable
{
public:
    void invoke() override {}
};

namespace sp
{

////
// EnoteScanningContextLedgerTEST
// - enote scanning context for injecting behavior into a scanning process
///
class EnoteScanningContextLedgerTEST final : public EnoteScanningContextLedger
{
public:
//constructors
    /// normal constructor
    EnoteScanningContextLedgerTEST(EnoteScanningContextLedgerSimple &core_scanning_context,
        Invocable &invocable_begin_scanning,
        Invocable &invocable_get_onchain_chunk,
        Invocable &invocable_get_unconfirmed_chunk,
        Invocable &invocable_terminate) :
            m_core_scanning_context{core_scanning_context},
            m_invocable_begin_scanning{invocable_begin_scanning},
            m_invocable_get_onchain_chunk{invocable_get_onchain_chunk},
            m_invocable_get_unconfirmed_chunk{invocable_get_unconfirmed_chunk},
            m_invocable_terminate{invocable_terminate}
    {}

//overloaded operators
    /// disable copy/move (this is a scoped manager [reference werapper])
    EnoteScanningContextLedgerTEST& operator=(EnoteScanningContextLedgerTEST&&) = delete;

//member functions
    /// tell the enote finder it can start scanning from a specified block height
    void begin_scanning_from_height(const std::uint64_t initial_start_height,
        const std::uint64_t max_chunk_size) override
    {
        m_invocable_begin_scanning.invoke();
        m_core_scanning_context.begin_scanning_from_height(initial_start_height, max_chunk_size);
    }
    /// get the next available onchain chunk (must be contiguous with the last chunk acquired since starting to scan)
    /// note: if chunk is empty, chunk represents top of current chain
    void get_onchain_chunk(EnoteScanningChunkLedgerV1 &chunk_out) override
    {
        m_invocable_get_onchain_chunk.invoke();
        m_core_scanning_context.get_onchain_chunk(chunk_out);
    }
    /// try to get a scanning chunk for the unconfirmed txs in a ledger
    bool try_get_unconfirmed_chunk(EnoteScanningChunkNonLedgerV1 &chunk_out) override
    {
        m_invocable_get_unconfirmed_chunk.invoke();
        return m_core_scanning_context.try_get_unconfirmed_chunk(chunk_out);
    }
    /// tell the enote finder to stop its scanning process (should be no-throw no-fail)
    void terminate_scanning() override
    {
        m_invocable_terminate.invoke();
        m_core_scanning_context.terminate_scanning();
    }

private:
    /// enote scanning context that this test context wraps
    EnoteScanningContextLedgerSimple &m_core_scanning_context;

    /// injected invocable objects
    Invocable &m_invocable_begin_scanning;
    Invocable &m_invocable_get_onchain_chunk;
    Invocable &m_invocable_get_unconfirmed_chunk;
    Invocable &m_invocable_terminate;
};

} //namespace sp


//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static crypto::secret_key make_secret_key()
{
    return rct::rct2sk(rct::skGen());
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void make_random_address_for_user(const sp::jamtis::jamtis_mock_keys &user_keys,
    sp::jamtis::JamtisDestinationV1 &user_address_out)
{
    using namespace sp;
    using namespace jamtis;

    address_index_t address_index;
    address_index.gen();

    ASSERT_NO_THROW(make_jamtis_destination_v1(user_keys.K_1_base,
        user_keys.K_ua,
        user_keys.K_fr,
        user_keys.s_ga,
        address_index,
        user_address_out));
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void convert_outlay_to_payment_proposal(const rct::xmr_amount outlay_amount,
    const sp::jamtis::JamtisDestinationV1 &destination,
    const sp::TxExtra &partial_memo_for_destination,
    sp::jamtis::JamtisPaymentProposalV1 &payment_proposal_out)
{
    using namespace sp;
    using namespace jamtis;

    payment_proposal_out = JamtisPaymentProposalV1{
            .m_destination = destination,
            .m_amount = outlay_amount,
            .m_enote_ephemeral_privkey = make_secret_key(),
            .m_partial_memo = partial_memo_for_destination
        };
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void add_coinbase_enotes_for_user(const rct::key &mock_input_context,
    const std::vector<rct::xmr_amount> &coinbase_amounts,
    const sp::jamtis::JamtisDestinationV1 &user_address,
    std::vector<sp::SpEnoteV1> &coinbase_enotes_inout,
    sp::SpTxSupplementV1 &tx_supplement_inout)
{
    using namespace sp;
    using namespace jamtis;

    // prepare mock coinbase enotes
    JamtisPaymentProposalV1 payment_proposal_temp;
    coinbase_enotes_inout.reserve(coinbase_enotes_inout.size() + coinbase_amounts.size());
    tx_supplement_inout.m_output_enote_ephemeral_pubkeys.reserve(
        tx_supplement_inout.m_output_enote_ephemeral_pubkeys.size() + coinbase_amounts.size());

    for (const rct::xmr_amount coinbase_amount : coinbase_amounts)
    {
        // make payment proposal
        convert_outlay_to_payment_proposal(coinbase_amount, user_address, TxExtra{}, payment_proposal_temp);

        // get output proposal
        SpOutputProposalV1 output_proposal;
        payment_proposal_temp.get_output_proposal_v1(mock_input_context, output_proposal);

        // save enote and ephemeral pubkey
        coinbase_enotes_inout.emplace_back();
        output_proposal.get_enote_v1(coinbase_enotes_inout.back());
        tx_supplement_inout.m_output_enote_ephemeral_pubkeys.emplace_back(output_proposal.m_enote_ephemeral_pubkey);
    }
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void send_coinbase_amounts_to_users(const std::vector<std::vector<rct::xmr_amount>> &coinbase_amounts_per_user,
    const std::vector<sp::jamtis::JamtisDestinationV1> &user_addresses,
    sp::MockLedgerContext &ledger_context_inout)
{
    ASSERT_TRUE(coinbase_amounts_per_user.size() == user_addresses.size());

    using namespace sp;
    using namespace jamtis;

    // prepare mock coinbase enotes
    const rct::key mock_input_context{rct::pkGen()};
    std::vector<SpEnoteV1> coinbase_enotes;
    SpTxSupplementV1 tx_supplement;

    for (std::size_t user_index{0}; user_index < user_addresses.size(); ++user_index)
    {
        add_coinbase_enotes_for_user(mock_input_context,
            coinbase_amounts_per_user[user_index],
            user_addresses[user_index],
            coinbase_enotes,
            tx_supplement);
    }

    // commit coinbase enotes as new block
    ASSERT_NO_THROW(ledger_context_inout.commit_unconfirmed_txs_v1(mock_input_context,
        std::move(tx_supplement),
        std::move(coinbase_enotes)));
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void refresh_user_enote_store(const sp::jamtis::jamtis_mock_keys &user_keys,
    const sp::RefreshLedgerEnoteStoreConfig &refresh_config,
    const sp::MockLedgerContext &ledger_context,
    sp::SpEnoteStoreMockV1 &user_enote_store_inout)
{
    using namespace sp;
    using namespace jamtis;

    const EnoteFindingContextLedgerMock enote_finding_context{ledger_context, user_keys.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context{enote_finding_context};
    EnoteStoreUpdaterLedgerMock enote_store_updater{user_keys.K_1_base, user_keys.k_vb, user_enote_store_inout};

    ASSERT_NO_THROW(refresh_enote_store_ledger(refresh_config, enote_scanning_context, enote_store_updater));
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void refresh_user_enote_store_PV(const sp::jamtis::jamtis_mock_keys &user_keys,
    const sp::RefreshLedgerEnoteStoreConfig &refresh_config,
    const sp::MockLedgerContext &ledger_context,
    sp::SpEnoteStoreMockPaymentValidatorV1 &user_enote_store_inout)
{
    using namespace sp;
    using namespace jamtis;

    const EnoteFindingContextLedgerMock enote_finding_context{ledger_context, user_keys.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context{enote_finding_context};
    EnoteStoreUpdaterLedgerMockIntermediate enote_store_updater{
            user_keys.K_1_base,
            user_keys.k_ua,
            user_keys.k_fr,
            user_keys.s_ga,
            user_enote_store_inout
        };

    ASSERT_NO_THROW(refresh_enote_store_ledger(refresh_config, enote_scanning_context, enote_store_updater));
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void construct_tx_for_mock_ledger_v1(const sp::jamtis::jamtis_mock_keys &local_user_keys,
    const sp::InputSelectorV1 &local_user_input_selector,
    const sp::FeeCalculator &tx_fee_calculator,
    const rct::xmr_amount fee_per_tx_weight,
    const std::size_t max_inputs,
    const std::vector<std::tuple<rct::xmr_amount, sp::jamtis::JamtisDestinationV1, sp::TxExtra>> &outlays,
    const std::size_t ref_set_decomp_n,
    const std::size_t ref_set_decomp_m,
    const sp::SpBinnedReferenceSetConfigV1 &bin_config,
    sp::MockLedgerContext &ledger_context_inout,
    sp::SpTxSquashedV1 &tx_out)
{
    using namespace sp;
    using namespace jamtis;

    /// build transaction

    // 1. prepare dummy and change addresses
    JamtisDestinationV1 change_address;
    JamtisDestinationV1 dummy_address;
    make_random_address_for_user(local_user_keys, change_address);
    make_random_address_for_user(local_user_keys, dummy_address);

    // 2. convert outlays to normal payment proposals
    std::vector<JamtisPaymentProposalV1> normal_payment_proposals;
    normal_payment_proposals.reserve(outlays.size());

    for (const auto &outlay : outlays)
    {
        normal_payment_proposals.emplace_back();

        convert_outlay_to_payment_proposal(std::get<rct::xmr_amount>(outlay),
            std::get<JamtisDestinationV1>(outlay),
            std::get<TxExtra>(outlay),
            normal_payment_proposals.back());
    }

    // 2. tx proposal
    SpTxProposalV1 tx_proposal;
    std::unordered_map<crypto::key_image, std::uint64_t> input_ledger_mappings;
    ASSERT_NO_THROW(ASSERT_TRUE(try_make_v1_tx_proposal_for_transfer_v1(local_user_keys.k_vb,
        change_address,
        dummy_address,
        local_user_input_selector,
        tx_fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        std::move(normal_payment_proposals),
        std::vector<JamtisPaymentProposalSelfSendV1>{},
        TxExtra{},
        tx_proposal,
        input_ledger_mappings)));

    // 3. prepare for membership proofs
    std::vector<SpMembershipProofPrepV1> membership_proof_preps;
    ASSERT_NO_THROW(make_mock_sp_membership_proof_preps_for_inputs_v1(input_ledger_mappings,
        tx_proposal.m_input_proposals,
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_inout,
        membership_proof_preps));

    // 4. complete tx
    ASSERT_NO_THROW(make_seraphis_tx_squashed_v1(tx_proposal,
        std::move(membership_proof_preps),
        SpTxSquashedV1::SemanticRulesVersion::MOCK,
        local_user_keys.k_m,
        local_user_keys.k_vb,
        tx_out));
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void transfer_funds_single_mock_v1_unconfirmed(const sp::jamtis::jamtis_mock_keys &local_user_keys,
    const sp::InputSelectorV1 &local_user_input_selector,
    const sp::FeeCalculator &tx_fee_calculator,
    const rct::xmr_amount fee_per_tx_weight,
    const std::size_t max_inputs,
    const std::vector<std::tuple<rct::xmr_amount, sp::jamtis::JamtisDestinationV1, sp::TxExtra>> &outlays,
    const std::size_t ref_set_decomp_n,
    const std::size_t ref_set_decomp_m,
    const sp::SpBinnedReferenceSetConfigV1 &bin_config,
    sp::MockLedgerContext &ledger_context_inout)
{
    using namespace sp;
    using namespace jamtis;

    // make one tx
    SpTxSquashedV1 single_tx;
    construct_tx_for_mock_ledger_v1(local_user_keys,
        local_user_input_selector,
        tx_fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        outlays,
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_inout,
        single_tx);

    // validate and submit to the mock ledger
    const sp::TxValidationContextMock tx_validation_context{ledger_context_inout};
    ASSERT_TRUE(validate_tx(single_tx, tx_validation_context));
    ASSERT_TRUE(ledger_context_inout.try_add_unconfirmed_tx_v1(single_tx));
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
TEST(seraphis_enote_scanning, trivial_ledger)
{
    using namespace sp;
    using namespace jamtis;

    // make user keys
    jamtis_mock_keys user_keys;
    make_jamtis_mock_keys(user_keys);

    // make user address
    address_index_t j;
    j.gen();
    JamtisDestinationV1 user_address;

    ASSERT_NO_THROW(make_jamtis_destination_v1(user_keys.K_1_base,
        user_keys.K_ua,
        user_keys.K_fr,
        user_keys.s_ga,
        j,
        user_address));

    // make enote for user
    const rct::xmr_amount enote_amount{1};
    const rct::key mock_input_context{rct::skGen()};
    SpTxSupplementV1 mock_tx_supplement{};

    const JamtisPaymentProposalV1 payment_proposal{
            .m_destination = user_address,
            .m_amount = enote_amount,
            .m_enote_ephemeral_privkey = make_secret_key(),
            .m_partial_memo = mock_tx_supplement.m_tx_extra
        };
    SpOutputProposalV1 output_proposal;
    payment_proposal.get_output_proposal_v1(mock_input_context, output_proposal);

    SpEnoteV1 single_enote;
    output_proposal.get_enote_v1(single_enote);
    mock_tx_supplement.m_output_enote_ephemeral_pubkeys.emplace_back(output_proposal.m_enote_ephemeral_pubkey);

    // add enote to mock ledger context as a coinbase enote
    MockLedgerContext ledger_context;
    ASSERT_NO_THROW(ledger_context.commit_unconfirmed_txs_v1(mock_input_context, mock_tx_supplement, {single_enote}));

    // make and refresh enote store with mock ledger context
    SpEnoteStoreMockV1 user_enote_store{0};
    const RefreshLedgerEnoteStoreConfig refresh_config{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 0
        };
    const EnoteFindingContextLedgerMock enote_finding_context{ledger_context, user_keys.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context{enote_finding_context};
    EnoteStoreUpdaterLedgerMock enote_store_updater{user_keys.K_1_base, user_keys.k_vb, user_enote_store};

    ASSERT_NO_THROW(refresh_enote_store_ledger(refresh_config, enote_scanning_context, enote_store_updater));

    // make a copy of the expected enote record
    SpEnoteRecordV1 single_enote_record;

    ASSERT_TRUE(try_get_enote_record_v1(single_enote,
        output_proposal.m_enote_ephemeral_pubkey,
        mock_input_context,
        user_keys.K_1_base,
        user_keys.k_vb,
        single_enote_record));

    // expect the enote to be found
    ASSERT_TRUE(user_enote_store.has_enote_with_key_image(single_enote_record.m_key_image));
}
//-------------------------------------------------------------------------------------------------------------------
TEST(seraphis_enote_scanning, simple_ledger)
{
    using namespace sp;
    using namespace jamtis;

    /// setup

    // 1. config
    const RefreshLedgerEnoteStoreConfig refresh_config{
            .m_reorg_avoidance_depth = 0,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 0
        };

    // 2. user keys
    jamtis_mock_keys user_keys_A;
    jamtis_mock_keys user_keys_B;
    make_jamtis_mock_keys(user_keys_A);
    make_jamtis_mock_keys(user_keys_B);

    // 3. user addresses
    JamtisDestinationV1 destination_A;
    JamtisDestinationV1 destination_B;
    make_random_address_for_user(user_keys_A, destination_A);
    make_random_address_for_user(user_keys_B, destination_B);


    /// tests

    // 1. one coinbase to user
    MockLedgerContext ledger_context_test1;
    SpEnoteStoreMockV1 enote_store_A_test1{0};
    send_coinbase_amounts_to_users({{1}}, {destination_A}, ledger_context_test1);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test1, enote_store_A_test1);

    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 1);

    // 2. two coinbase to user (one coinbase tx)
    MockLedgerContext ledger_context_test2;
    SpEnoteStoreMockV1 enote_store_A_test2{0};
    send_coinbase_amounts_to_users({{1, 1}}, {destination_A}, ledger_context_test2);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test2, enote_store_A_test2);

    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 2);

    // 3. two coinbase owned by different users (one coinbase tx)
    MockLedgerContext ledger_context_test3;
    SpEnoteStoreMockV1 enote_store_A_test3{0};
    SpEnoteStoreMockV1 enote_store_B_test3{0};
    send_coinbase_amounts_to_users({{1}, {2}}, {destination_A, destination_B}, ledger_context_test3);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test3, enote_store_A_test3);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test3, enote_store_B_test3);

    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 1);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 2);

    // 4. two coinbase to user, search between each send (two coinbase txs i.e. two blocks)
    MockLedgerContext ledger_context_test4;
    SpEnoteStoreMockV1 enote_store_A_test4{0};
    send_coinbase_amounts_to_users({{1}}, {destination_A}, ledger_context_test4);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 1);

    send_coinbase_amounts_to_users({{2}}, {destination_A}, ledger_context_test4);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 3);

    // 5. search once, three coinbase to user, search once, pop 2, search again, 1 coinbase to user, search again
    const RefreshLedgerEnoteStoreConfig refresh_config_test5{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 0
        };
    MockLedgerContext ledger_context_test5;
    SpEnoteStoreMockV1 enote_store_A_test5{0};
    refresh_user_enote_store(user_keys_A, refresh_config_test5, ledger_context_test5, enote_store_A_test5);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);

    send_coinbase_amounts_to_users({{1}}, {destination_A}, ledger_context_test5);
    send_coinbase_amounts_to_users({{2}}, {destination_A}, ledger_context_test5);
    send_coinbase_amounts_to_users({{4}}, {destination_A}, ledger_context_test5);
    refresh_user_enote_store(user_keys_A, refresh_config_test5, ledger_context_test5, enote_store_A_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 7);

    ledger_context_test5.pop_blocks(2);
    refresh_user_enote_store(user_keys_A, refresh_config_test5, ledger_context_test5, enote_store_A_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 1);

    send_coinbase_amounts_to_users({{8}}, {destination_A}, ledger_context_test5);
    refresh_user_enote_store(user_keys_A, refresh_config_test5, ledger_context_test5, enote_store_A_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 9);

    // 6. search, three coinbase to user, search, pop 2, search, 1 coinbase to user, search, pop 3, search
    // - refresh height 1
    const RefreshLedgerEnoteStoreConfig refresh_config_test6{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 0
        };
    MockLedgerContext ledger_context_test6;
    SpEnoteStoreMockV1 enote_store_A_test6{1};
    refresh_user_enote_store(user_keys_A, refresh_config_test6, ledger_context_test6, enote_store_A_test6);

    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);

    send_coinbase_amounts_to_users({{1}}, {destination_A}, ledger_context_test6);
    send_coinbase_amounts_to_users({{2}}, {destination_A}, ledger_context_test6);
    send_coinbase_amounts_to_users({{4}}, {destination_A}, ledger_context_test6);
    refresh_user_enote_store(user_keys_A, refresh_config_test6, ledger_context_test6, enote_store_A_test6);

    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 6);

    ledger_context_test6.pop_blocks(2);
    refresh_user_enote_store(user_keys_A, refresh_config_test6, ledger_context_test6, enote_store_A_test6);

    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);

    send_coinbase_amounts_to_users({{8}}, {destination_A}, ledger_context_test6);
    refresh_user_enote_store(user_keys_A, refresh_config_test6, ledger_context_test6, enote_store_A_test6);

    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 8);

    ledger_context_test6.pop_blocks(3);
    refresh_user_enote_store(user_keys_A, refresh_config_test6, ledger_context_test6, enote_store_A_test6);

    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::OFFCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_OFFCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test6.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
}
//-------------------------------------------------------------------------------------------------------------------
TEST(seraphis_enote_scanning, basic_ledger_tx_passing)
{
    using namespace sp;
    using namespace jamtis;

    /// setup

    // 1. config
    const std::size_t max_inputs{1000};
    const std::size_t fee_per_tx_weight{0};  // 0 fee here
    const std::size_t ref_set_decomp_n{2};
    const std::size_t ref_set_decomp_m{2};

    const RefreshLedgerEnoteStoreConfig refresh_config{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 0
        };

    const FeeCalculatorMockTrivial fee_calculator;  //just do a trivial calculator here (fee = fee/weight * 1 weight)

    const SpBinnedReferenceSetConfigV1 bin_config{
            .m_bin_radius = 1,
            .m_num_bin_members = 2
        };

    // 2. user keys
    jamtis_mock_keys user_keys_A;
    jamtis_mock_keys user_keys_B;
    make_jamtis_mock_keys(user_keys_A);
    make_jamtis_mock_keys(user_keys_B);

    // 3. user addresses
    JamtisDestinationV1 destination_A;
    JamtisDestinationV1 destination_B;
    make_random_address_for_user(user_keys_A, destination_A);
    make_random_address_for_user(user_keys_B, destination_B);


    /// tests

    // 1. one unconfirmed tx (no change), then commit it (include payment validator checks)
    MockLedgerContext ledger_context_test1;
    SpEnoteStoreMockV1 enote_store_A_test1{0};
    SpEnoteStoreMockPaymentValidatorV1 enote_store_PV_A_test1{0};
    SpEnoteStoreMockV1 enote_store_B_test1{0};
    const sp::InputSelectorMockV1 input_selector_A_test1{enote_store_A_test1};
    const sp::InputSelectorMockV1 input_selector_B_test1{enote_store_B_test1};
    send_coinbase_amounts_to_users({{1, 1, 1, 1}}, {destination_A}, ledger_context_test1);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test1, enote_store_A_test1);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test1,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{2, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test1);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test1, enote_store_A_test1);
    refresh_user_enote_store_PV(user_keys_A, refresh_config, ledger_context_test1, enote_store_PV_A_test1);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test1, enote_store_B_test1);

    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 4);
    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 2);
    ASSERT_TRUE(enote_store_PV_A_test1.get_received_sum({SpEnoteOriginStatus::OFFCHAIN,
        SpEnoteOriginStatus::UNCONFIRMED}) == 0);  //can't find change
    ASSERT_TRUE(enote_store_PV_A_test1.get_received_sum({SpEnoteOriginStatus::ONCHAIN}) == 4);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 2);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 2);

    ledger_context_test1.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test1, enote_store_A_test1);
    refresh_user_enote_store_PV(user_keys_A, refresh_config, ledger_context_test1, enote_store_PV_A_test1);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test1, enote_store_B_test1);

    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 2);
    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 2);
    ASSERT_TRUE(enote_store_PV_A_test1.get_received_sum({SpEnoteOriginStatus::OFFCHAIN,
        SpEnoteOriginStatus::UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_PV_A_test1.get_received_sum({SpEnoteOriginStatus::ONCHAIN}) == 4); //coinbase + can't find change
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 2);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 2);

    // 2. one unconfirmed tx (>0 change), then commit it
    MockLedgerContext ledger_context_test2;
    SpEnoteStoreMockV1 enote_store_A_test2{0};
    SpEnoteStoreMockV1 enote_store_B_test2{0};
    const sp::InputSelectorMockV1 input_selector_A_test2{enote_store_A_test2};
    const sp::InputSelectorMockV1 input_selector_B_test2{enote_store_B_test2};
    send_coinbase_amounts_to_users({{0, 0, 0, 8}}, {destination_A}, ledger_context_test2);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test2, enote_store_A_test2);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test2,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{3, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test2);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test2, enote_store_A_test2);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test2, enote_store_B_test2);

    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 8);
    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 5);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 3);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 3);

    ledger_context_test2.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test2, enote_store_A_test2);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test2, enote_store_B_test2);

    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 5);
    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 5);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 3);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 3);

    // 3. one unconfirmed tx (>0 change), then commit it + coinbase to B
    MockLedgerContext ledger_context_test3;
    SpEnoteStoreMockV1 enote_store_A_test3{0};
    SpEnoteStoreMockV1 enote_store_B_test3{0};
    const sp::InputSelectorMockV1 input_selector_A_test3{enote_store_A_test3};
    const sp::InputSelectorMockV1 input_selector_B_test3{enote_store_B_test3};
    send_coinbase_amounts_to_users({{0, 0, 0, 8}}, {destination_A}, ledger_context_test3);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test3, enote_store_A_test3);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test3,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{3, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test3);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test3, enote_store_A_test3);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test3, enote_store_B_test3);

    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 8);
    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 5);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 3);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 3);

    send_coinbase_amounts_to_users({{8}}, {destination_B}, ledger_context_test3);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test3, enote_store_A_test3);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test3, enote_store_B_test3);

    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 5);
    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 5);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 11);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 11);

    // 4. pass funds around with unconfirmed cache clear
    MockLedgerContext ledger_context_test4;
    SpEnoteStoreMockV1 enote_store_A_test4{0};
    SpEnoteStoreMockV1 enote_store_B_test4{0};
    const sp::InputSelectorMockV1 input_selector_A_test4{enote_store_A_test4};
    const sp::InputSelectorMockV1 input_selector_B_test4{enote_store_B_test4};
    send_coinbase_amounts_to_users({{10, 10, 10, 10}}, {destination_A}, ledger_context_test4);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test4,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{20, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test4);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test4, enote_store_B_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 40);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 20);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 20);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 20);

    ledger_context_test4.clear_unconfirmed_cache();
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test4, enote_store_B_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 40);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 40);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test4,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{30, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test4);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test4, enote_store_B_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 40);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 10);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 30);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 30);

    ledger_context_test4.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test4, enote_store_B_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 10);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 10);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 30);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 30);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test4,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{3, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test4);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test4, enote_store_B_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 10);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 7);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 30);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 3);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 33);

    ledger_context_test4.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test4, enote_store_A_test4);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test4, enote_store_B_test4);

    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 7);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 7);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 33);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test4.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 33);

    // 5. pass funds around with non-zero refresh height and reorging
    MockLedgerContext ledger_context_test5;
    SpEnoteStoreMockV1 enote_store_A_test5{0};
    SpEnoteStoreMockV1 enote_store_B_test5{2};
    const sp::InputSelectorMockV1 input_selector_A_test5{enote_store_A_test5};
    const sp::InputSelectorMockV1 input_selector_B_test5{enote_store_B_test5};
    send_coinbase_amounts_to_users({{10, 10, 10, 10}}, {destination_A}, ledger_context_test5);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test5,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{11, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test5);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test5, enote_store_B_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 40);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 29);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 11);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 11);

    ledger_context_test5.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test5, enote_store_B_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 29);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 29);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test5,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{12, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test5);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test5, enote_store_B_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 29);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 17);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 12);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 12);

    ledger_context_test5.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test5, enote_store_B_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 17);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 17);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 12);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 12);

    ledger_context_test5.pop_blocks(1);
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test5, enote_store_B_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 29);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 29);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test5,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{13, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test5);

    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test5, enote_store_B_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 29);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 16);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 13);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 13);

    ledger_context_test5.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config, ledger_context_test5, enote_store_A_test5);
    refresh_user_enote_store(user_keys_B, refresh_config, ledger_context_test5, enote_store_B_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 16);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 16);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 13);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 13);
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
class InvocableTest1 final : public Invocable
{
public:
    InvocableTest1(sp::MockLedgerContext &ledger_context) : m_ledger_contex{ledger_context} {}
    InvocableTest1& operator=(InvocableTest1&&) = delete;

    /// invoke: on the third call, pop 2 blocks from the ledger context
    void invoke() override
    {
        ++m_num_calls;

        if (m_num_calls == 3)
            m_ledger_contex.pop_blocks(2);
    }
private:
    sp::MockLedgerContext &m_ledger_contex;
    std::size_t m_num_calls{0};
};
//-------------------------------------------------------------------------------------------------------------------
class InvocableTest2 final : public Invocable
{
public:
    InvocableTest2(const sp::jamtis::JamtisDestinationV1 &user_address,
        std::vector<rct::xmr_amount> amounts_per_new_coinbase,
        sp::MockLedgerContext &ledger_context) :
            m_user_address{user_address},
            m_amounts_per_new_coinbase{std::move(amounts_per_new_coinbase)},
            m_ledger_contex{ledger_context}
    {}
    InvocableTest2& operator=(InvocableTest2&&) = delete;

    /// invoke: on the first call, pop 2 blocks then push back N new blocks with one coinbase amount each
    void invoke() override
    {
        ++m_num_calls;

        if (m_num_calls == 1)
        {
            m_ledger_contex.pop_blocks(2);
            for (const rct::xmr_amount new_coinbase_amount : m_amounts_per_new_coinbase)
                send_coinbase_amounts_to_users({{new_coinbase_amount}}, {m_user_address}, m_ledger_contex);
        }
    }
private:
    const sp::jamtis::JamtisDestinationV1 &m_user_address;
    const std::vector<rct::xmr_amount> m_amounts_per_new_coinbase;
    sp::MockLedgerContext &m_ledger_contex;
    std::size_t m_num_calls{0};
};
//-------------------------------------------------------------------------------------------------------------------
class InvocableTest3 final : public Invocable
{
public:
    InvocableTest3(const sp::jamtis::JamtisDestinationV1 &user_address,
        std::vector<rct::xmr_amount> amounts_per_new_coinbase,
        sp::MockLedgerContext &ledger_context) :
            m_user_address{user_address},
            m_amounts_per_new_coinbase{std::move(amounts_per_new_coinbase)},
            m_ledger_contex{ledger_context}
    {}
    InvocableTest3& operator=(InvocableTest3&&) = delete;

    /// invoke: on the third call, pop 2 blocks then push back N new blocks with one coinbase amount each
    void invoke() override
    {
        ++m_num_calls;

        if (m_num_calls == 3)
        {
            m_ledger_contex.pop_blocks(2);
            for (const rct::xmr_amount new_coinbase_amount : m_amounts_per_new_coinbase)
                send_coinbase_amounts_to_users({{new_coinbase_amount}}, {m_user_address}, m_ledger_contex);
        }
    }

    /// return number of invocations
    std::size_t num_invocations() const { return m_num_calls; }
private:
    const sp::jamtis::JamtisDestinationV1 &m_user_address;
    const std::vector<rct::xmr_amount> m_amounts_per_new_coinbase;
    sp::MockLedgerContext &m_ledger_contex;
    std::size_t m_num_calls{0};
};
//-------------------------------------------------------------------------------------------------------------------
class InvocableTest4 final : public Invocable
{
public:
    InvocableTest4(const sp::jamtis::JamtisDestinationV1 &user_address,
        const rct::xmr_amount amount_new_coinbase,
        sp::MockLedgerContext &ledger_context) :
            m_user_address{user_address},
            m_amount_new_coinbase{amount_new_coinbase},
            m_ledger_contex{ledger_context}
    {}
    InvocableTest4& operator=(InvocableTest4&&) = delete;

    /// invoke: on every third call, pop 1 block then push back 1 new block with one coinbase amount
    void invoke() override
    {
        ++m_num_calls;

        if (m_num_calls % 3 == 0)
        {
            m_ledger_contex.pop_blocks(1);
            send_coinbase_amounts_to_users({{m_amount_new_coinbase}}, {m_user_address}, m_ledger_contex);
        }
    }
private:
    const sp::jamtis::JamtisDestinationV1 &m_user_address;
    const rct::xmr_amount m_amount_new_coinbase;
    sp::MockLedgerContext &m_ledger_contex;
    std::size_t m_num_calls{0};
};
//-------------------------------------------------------------------------------------------------------------------
class InvocableTest5Submit final : public Invocable
{
public:
    InvocableTest5Submit(sp::SpTxSquashedV1 tx_to_submit,
        sp::MockLedgerContext &ledger_context) :
            m_tx_to_submit{std::move(tx_to_submit)},
            m_ledger_contex{ledger_context}
    {}
    InvocableTest5Submit& operator=(InvocableTest5Submit&&) = delete;

    /// invoke: on the first call, submit prepared tx to the unconfirmed cache of the ledger
    void invoke() override
    {
        ++m_num_calls;

        if (m_num_calls == 1)
        {
            // validate and submit to the mock ledger
            const sp::TxValidationContextMock tx_validation_context{m_ledger_contex};
            ASSERT_TRUE(validate_tx(m_tx_to_submit, tx_validation_context));
            ASSERT_TRUE(m_ledger_contex.try_add_unconfirmed_tx_v1(m_tx_to_submit));
        }
    }
private:
    const sp::SpTxSquashedV1 m_tx_to_submit;
    sp::MockLedgerContext &m_ledger_contex;
    std::size_t m_num_calls{0};
};
//-------------------------------------------------------------------------------------------------------------------
class InvocableTest5Commit final : public Invocable
{
public:
    InvocableTest5Commit(sp::MockLedgerContext &ledger_context) : m_ledger_contex{ledger_context} {}
    InvocableTest5Commit& operator=(InvocableTest5Commit&&) = delete;

    /// invoke: commit any unconfirmed txs in the ledger's unconfirmed chache
    void invoke() override
    {
        m_ledger_contex.commit_unconfirmed_txs_v1(rct::key{}, sp::SpTxSupplementV1{}, std::vector<sp::SpEnoteV1>{});
    }
private:
    sp::MockLedgerContext &m_ledger_contex;
};
//-------------------------------------------------------------------------------------------------------------------
TEST(seraphis_enote_scanning, reorgs_while_scanning)
{
    using namespace sp;
    using namespace jamtis;

    /// setup
    DummyInvocable dummy_invocable;

    // 1. config
    const std::size_t max_inputs{1000};
    const std::size_t fee_per_tx_weight{0};  // 0 fee here
    const std::size_t ref_set_decomp_n{2};
    const std::size_t ref_set_decomp_m{2};

    const FeeCalculatorMockTrivial fee_calculator;  //just do a trivial calculator here (fee = fee/weight * 1 weight)

    const SpBinnedReferenceSetConfigV1 bin_config{
            .m_bin_radius = 1,
            .m_num_bin_members = 2
        };

    // 2. user keys
    jamtis_mock_keys user_keys_A;
    jamtis_mock_keys user_keys_B;
    make_jamtis_mock_keys(user_keys_A);
    make_jamtis_mock_keys(user_keys_B);

    // 3. user addresses
    JamtisDestinationV1 destination_A;
    JamtisDestinationV1 destination_B;
    make_random_address_for_user(user_keys_A, destination_A);
    make_random_address_for_user(user_keys_B, destination_B);


    /// tests

    // 1. full internal reorg
    const RefreshLedgerEnoteStoreConfig refresh_config_test1{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 0
        };
    MockLedgerContext ledger_context_test1;
    SpEnoteStoreMockV1 enote_store_A_test1{0};
    SpEnoteStoreMockV1 enote_store_B_test1{0};
    const sp::InputSelectorMockV1 input_selector_A_test1{enote_store_A_test1};
    const sp::InputSelectorMockV1 input_selector_B_test1{enote_store_B_test1};
    send_coinbase_amounts_to_users({{1, 1, 1, 1}}, {destination_A}, ledger_context_test1);

    // a. refresh once so alignment will begin on block 0 in the test
    refresh_user_enote_store(user_keys_A, refresh_config_test1, ledger_context_test1, enote_store_A_test1);

    // b. send tx A -> B
    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test1,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{2, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test1);
    ledger_context_test1.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});

    // c. refresh user A with injected invocable
    // current chain state: {block0[{1, 1, 1, 1} -> A], block1[A -> {2} -> B]}
    // current enote context A: [enotes: block0{1, 1, 1, 1}], [blocks: 0{...}]
    // expected refresh sequence:
    // 1. desired start height = block 1
    // 2. actual start height = block 0 = ([desired start] 1 - [reorg depth] 1)
    // 3. scan process
    //   a. onchain loop
    //     i.   get onchain chunk: block 0  (success: chunk range [0, 1))
    //     ii.  get onchain chunk: block 1  (success: chunk range [1, 2))
    //     iii. get onchain chunk: block 2  (injected: pop 2)  (fail: chunk range [0,0) -> NEED_FULLSCAN)
    //   b. skip unconfirmed chunk: (NEED_FULLSCAN)
    // 4. NEED_FULLSCAN: rescan from block 0
    //   a. onchain loop
    //     i.   get onchain chunk: block 0  (success: chunk range [0, 0) -> DONE)
    //   b. unconfirmed chunk: empty
    //   c. follow-up onchain loop: success on block 0 (range [0, 0) -> DONE)
    // 5. DONE: refresh enote store of A
    const EnoteFindingContextLedgerMock enote_finding_context_A_test1{ledger_context_test1, user_keys_A.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context_A_test1{enote_finding_context_A_test1};
    InvocableTest1 invocable_get_onchain_test1{ledger_context_test1};
    EnoteScanningContextLedgerTEST test_scanning_context_A_test1(enote_scanning_context_A_test1,
        dummy_invocable,
        invocable_get_onchain_test1,
        dummy_invocable,
        dummy_invocable);
    EnoteStoreUpdaterLedgerMock enote_store_updater_test1{user_keys_A.K_1_base, user_keys_A.k_vb, enote_store_A_test1};
    ASSERT_NO_THROW(refresh_enote_store_ledger(refresh_config_test1,
        test_scanning_context_A_test1,
        enote_store_updater_test1));

    // d. after refreshing, both users should have no balance
    refresh_user_enote_store(user_keys_B, refresh_config_test1, ledger_context_test1, enote_store_B_test1);

    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test1.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test1.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);

    // 2. full internal reorg with replacement
    const RefreshLedgerEnoteStoreConfig refresh_config_test2{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 0
        };
    MockLedgerContext ledger_context_test2;
    SpEnoteStoreMockV1 enote_store_A_test2{0};
    SpEnoteStoreMockV1 enote_store_B_test2{0};
    const sp::InputSelectorMockV1 input_selector_A_test2{enote_store_A_test2};
    const sp::InputSelectorMockV1 input_selector_B_test2{enote_store_B_test2};
    send_coinbase_amounts_to_users({{1, 1, 1, 1}}, {destination_A}, ledger_context_test2);

    // a. refresh A so coinbase funds are available
    refresh_user_enote_store(user_keys_A, refresh_config_test2, ledger_context_test2, enote_store_A_test2);

    // b. send two tx A -> B in two blocks
    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test2,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{1, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test2);
    ledger_context_test2.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config_test2, ledger_context_test2, enote_store_A_test2);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test2,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{2, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test2);
    ledger_context_test2.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});

    // c. refresh A so top block is block 2
    refresh_user_enote_store(user_keys_A, refresh_config_test2, ledger_context_test2, enote_store_A_test2);

    // d. refresh user A with injected invocable
    // current chain state: {block0[{1, 1, 1, 1} -> A], block1[A -> {1} -> B], block2[A -> {2} -> B]}
    // current enote context A: [enotes: block0{1, 1, 1, 1}, block1{0}, block2{0}], [blocks: 0{...}, 1{...}, 2{...}]
    // expected refresh sequence:
    // 1. desired start height = block 3
    // 2. actual start height = block 2 = ([desired start] 3 - [reorg depth] 1)
    // 3. scan process
    //   a. onchain loop
    //     i.   get onchain chunk: block 2  (injected: pop 2, +2 blocks)  (fail: chunk range [2, 3) -> NEED_FULLSCAN)
    //   b. skip unconfirmed chunk: (NEED_FULLSCAN)
    // 4. NEED_FULLSCAN: rescan from block 1
    //   a. onchain loop
    //     i.   get onchain chunk: block 1  (success: chunk range [1, 2))
    //     ii.  get onchain chunk: block 2  (success: chunk range [2, 3))
    //     iii. get onchain chunk: block 3  (success: chunk range [3, 3) -> DONE)
    //   b. unconfirmed chunk: empty
    //   c. follow-up onchain loop: success on block 3 (range [3, 3) -> DONE)
    // 5. DONE: refresh enote store of A
    const EnoteFindingContextLedgerMock enote_finding_context_A_test2{ledger_context_test2, user_keys_A.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context_A_test2{enote_finding_context_A_test2};
    InvocableTest2 invocable_get_onchain_test2{destination_A, {3, 5}, ledger_context_test2};
    EnoteScanningContextLedgerTEST test_scanning_context_A_test2(enote_scanning_context_A_test2,
        dummy_invocable,
        invocable_get_onchain_test2,
        dummy_invocable,
        dummy_invocable);
    EnoteStoreUpdaterLedgerMock enote_store_updater_test2{user_keys_A.K_1_base, user_keys_A.k_vb, enote_store_A_test2};
    ASSERT_NO_THROW(refresh_enote_store_ledger(refresh_config_test2,
        test_scanning_context_A_test2,
        enote_store_updater_test2));

    // d. check balances after refreshing
    refresh_user_enote_store(user_keys_B, refresh_config_test2, ledger_context_test2, enote_store_B_test2);

    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 12);
    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test2.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 12);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 0);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test2.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);

    // 3. partial internal reorg with replacement
    const RefreshLedgerEnoteStoreConfig refresh_config_test3{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 1
        };
    MockLedgerContext ledger_context_test3;
    SpEnoteStoreMockV1 enote_store_A_test3{0};
    SpEnoteStoreMockV1 enote_store_B_test3{0};
    const sp::InputSelectorMockV1 input_selector_A_test3{enote_store_A_test3};
    const sp::InputSelectorMockV1 input_selector_B_test3{enote_store_B_test3};
    send_coinbase_amounts_to_users({{1, 1, 1, 1}}, {destination_A}, ledger_context_test3);

    // a. refresh once so user A can make a tx
    refresh_user_enote_store(user_keys_A, refresh_config_test3, ledger_context_test3, enote_store_A_test3);

    // b. send two txs A -> B in two blocks
    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test3,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{1, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test3);
    ledger_context_test3.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config_test3, ledger_context_test3, enote_store_A_test3);

    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test3,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{2, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test3);
    ledger_context_test3.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});

    // c. refresh user B with injected invocable
    // current chain state: {block0[{2, 2, 2, 2} -> A], block1[A -> {1} -> B], block2[A -> {2} -> B]}
    // current enote context B: [enotes: none, [blocks: none]
    // expected refresh sequence:
    // 1. desired start height = block 0
    // 2. actual start height = block 0 = round_to_0([desired start] 0 - [reorg depth] 1)
    // 3. scan process
    //   a. onchain loop
    //     i.   get onchain chunk: block 0  (success: chunk range [0, 1))
    //     ii.  get onchain chunk: block 1  (success: chunk range [1, 2))
    //     iii. get onchain chunk: block 2  (injected: pop 2, +2 blocks)  (fail: chunk range [2, 3) -> NEED_PARTIALSCAN)
    //   b. skip unconfirmed chunk: (NEED_PARTIALSCAN)
    // 4. NEED_PARTIALSCAN: rescan from block 1 (desired block: 2, reorg depth: 1)
    //   a. onchain loop
    //     i.   get onchain chunk: block 1  (success: chunk range [1, 2))
    //     ii.  get onchain chunk: block 2  (success: chunk range [2, 3))
    //     iii. get onchain chunk: block 3  (success: chunk range [3, 3) -> DONE)
    //   b. unconfirmed chunk: empty
    //   c. follow-up onchain loop: success on block 3 (range [3, 3) -> DONE)
    // 5. DONE: refresh enote store of B
    const EnoteFindingContextLedgerMock enote_finding_context_B_test3{ledger_context_test3, user_keys_B.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context_B_test3{enote_finding_context_B_test3};
    InvocableTest3 invocable_get_onchain_test3{destination_B, {3, 5}, ledger_context_test3};
    EnoteScanningContextLedgerTEST test_scanning_context_B_test3(enote_scanning_context_B_test3,
        dummy_invocable,
        invocable_get_onchain_test3,
        dummy_invocable,
        dummy_invocable);
    EnoteStoreUpdaterLedgerMock enote_store_updater_test3{user_keys_B.K_1_base, user_keys_B.k_vb, enote_store_B_test3};
    ASSERT_NO_THROW(refresh_enote_store_ledger(refresh_config_test3,
        test_scanning_context_B_test3,
        enote_store_updater_test3));

    // d. make sure NEED_FULLSCAN was not triggered on the reorg (would be == 8 here because fullscan will rescan block 0)
    ASSERT_TRUE(invocable_get_onchain_test3.num_invocations() == 7);

    // e. check users' balances
    refresh_user_enote_store(user_keys_A, refresh_config_test3, ledger_context_test3, enote_store_A_test3);

    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 4);
    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test3.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 4);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 8);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test3.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 8);

    // 4. partial internal reorgs to failure
    const RefreshLedgerEnoteStoreConfig refresh_config_test4{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 4
        };
    MockLedgerContext ledger_context_test4;
    SpEnoteStoreMockV1 enote_store_A_test4{0};
    SpEnoteStoreMockV1 enote_store_B_test4{0};
    const sp::InputSelectorMockV1 input_selector_A_test4{enote_store_A_test4};
    const sp::InputSelectorMockV1 input_selector_B_test4{enote_store_B_test4};
    send_coinbase_amounts_to_users({{1, 1, 1, 1}}, {destination_A}, ledger_context_test4);

    // a. refresh once so user A can make a tx
    refresh_user_enote_store(user_keys_A, refresh_config_test4, ledger_context_test4, enote_store_A_test4);

    // b. send tx A -> B
    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test4,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{1, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test4);
    ledger_context_test4.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});

    // c. refresh user B with injected invocable
    // current chain state: {block0[{1, 1, 1, 1} -> A], block1[A -> {1} -> B]}
    // current enote context B: [enotes: none], [blocks: none]
    // expected refresh sequence:
    // 1. desired start height = block 0
    // 2. actual start height = block 0 = ([desired start] 0 - [reorg depth] 0)
    // 3. scan process
    //   a. onchain loop
    //     i.   get onchain chunk: block 0  (success: chunk range [0, 1))
    //     ii.  get onchain chunk: block 1  (success: chunk range [1, 2))
    //     iii. get onchain chunk: block 2  (inject: pop 1, +1 blocks) (fail: chunk range [2, 2) -> NEED_PARTIALSCAN)
    //   b. skip unconfirmed chunk: (NEED_PARTIALSCAN)
    // 4. NEED_PARTIALSCAN: rescan from block 0
    //   a. onchain loop
    //     i.   get onchain chunk: block 0  (success: chunk range [0, 1))
    //     ii.  get onchain chunk: block 1  (success: chunk range [1, 2))
    //     iii. get onchain chunk: block 2  (inject: pop 1, +1 blocks) (fail: chunk range [2, 2) -> NEED_PARTIALSCAN)
    //   b. skip unconfirmed chunk: (NEED_PARTIALSCAN)
    // 5. ... etc. until partialscan attempts runs out (then throw)
    const EnoteFindingContextLedgerMock enote_finding_context_B_test4{ledger_context_test4, user_keys_B.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context_B_test4{enote_finding_context_B_test4};
    InvocableTest4 invocable_get_onchain_test4{destination_B, 1, ledger_context_test4};
    EnoteScanningContextLedgerTEST test_scanning_context_B_test4(enote_scanning_context_B_test4,
        dummy_invocable,
        invocable_get_onchain_test4,
        dummy_invocable,
        dummy_invocable);
    EnoteStoreUpdaterLedgerMock enote_store_updater_test4{user_keys_B.K_1_base, user_keys_B.k_vb, enote_store_B_test4};
    ASSERT_ANY_THROW(refresh_enote_store_ledger(refresh_config_test4,
        test_scanning_context_B_test4,
        enote_store_updater_test4));

    // 5. sneaky tx found in follow-up loop
    const RefreshLedgerEnoteStoreConfig refresh_config_test5{
            .m_reorg_avoidance_depth = 1,
            .m_max_chunk_size = 1,
            .m_max_partialscan_attempts = 4
        };
    MockLedgerContext ledger_context_test5;
    SpEnoteStoreMockV1 enote_store_A_test5{0};
    SpEnoteStoreMockV1 enote_store_B_test5{0};
    const sp::InputSelectorMockV1 input_selector_A_test5{enote_store_A_test5};
    const sp::InputSelectorMockV1 input_selector_B_test5{enote_store_B_test5};
    send_coinbase_amounts_to_users({{1, 1, 1, 1}}, {destination_A}, ledger_context_test5);

    // a. refresh once so user A can make a tx
    refresh_user_enote_store(user_keys_A, refresh_config_test5, ledger_context_test5, enote_store_A_test5);

    // b. send tx A -> B
    transfer_funds_single_mock_v1_unconfirmed(user_keys_A,
        input_selector_A_test5,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{1, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test5);
    ledger_context_test5.commit_unconfirmed_txs_v1(rct::key{}, SpTxSupplementV1{}, std::vector<SpEnoteV1>{});
    refresh_user_enote_store(user_keys_A, refresh_config_test5, ledger_context_test5, enote_store_A_test5);

    // c. prepare sneaky tx to insert while scanning
    SpTxSquashedV1 sneaky_tx_test5;
    construct_tx_for_mock_ledger_v1(user_keys_A,
        input_selector_A_test5,
        fee_calculator,
        fee_per_tx_weight,
        max_inputs,
        {{2, destination_B, TxExtra{}}},
        ref_set_decomp_n,
        ref_set_decomp_m,
        bin_config,
        ledger_context_test5,
        sneaky_tx_test5);

    // c. refresh user B with injected invocable
    // current chain state: {block0[{1, 1, 1, 1} -> A], block1[A -> {1} -> B]}
    // current enote context B: [enotes: none], [blocks: none]
    // expected refresh sequence:
    // 1. desired start height = block 0
    // 2. actual start height = block 0 = ([desired start] 0 - [reorg depth] 0)
    // 3. scan process
    //   a. onchain loop
    //     i.   get onchain chunk: block 0  (success: chunk range [0, 1))
    //     ii.  get onchain chunk: block 1  (success: chunk range [1, 2))
    //     iii. get onchain chunk: block 2  (success: chunk range [2, 2) -> DONE)
    //   b. unconfirmed chunk: (inject: submit A -> {2} -> B)  (success: found {2})
    //   c. follow-up onchain loop
    //     i.   get onchain chunk: block 2  (inject: commit unconfirmed)  (success: chunk range [2, 3])
    //     ii.  get onchain chunk: block 3  (success: chunk range [3, 3) -> DONE)
    // 4. DONE: refresh enote store of B
    const EnoteFindingContextLedgerMock enote_finding_context_B_test5{ledger_context_test5, user_keys_B.k_fr};
    EnoteScanningContextLedgerSimple enote_scanning_context_B_test5{enote_finding_context_B_test5};
    InvocableTest5Submit invocable_get_onchain_test5{std::move(sneaky_tx_test5), ledger_context_test5};
    InvocableTest5Commit invocable_get_unconfirmed_test5{ledger_context_test5};
    EnoteScanningContextLedgerTEST test_scanning_context_B_test5(enote_scanning_context_B_test5,
        dummy_invocable,
        invocable_get_onchain_test5,
        invocable_get_unconfirmed_test5,
        dummy_invocable);
    EnoteStoreUpdaterLedgerMock enote_store_updater_test5{user_keys_B.K_1_base, user_keys_B.k_vb, enote_store_B_test5};
    ASSERT_NO_THROW(refresh_enote_store_ledger(refresh_config_test5,
        test_scanning_context_B_test5,
        enote_store_updater_test5));

    // d. check users' balances
    refresh_user_enote_store(user_keys_A, refresh_config_test5, ledger_context_test5, enote_store_A_test5);

    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 1);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_A_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 1);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN},
        {SpEnoteSpentStatus::SPENT_ONCHAIN}) == 3);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 0);
    ASSERT_TRUE(enote_store_B_test5.get_balance({SpEnoteOriginStatus::ONCHAIN, SpEnoteOriginStatus::UNCONFIRMED},
        {SpEnoteSpentStatus::SPENT_ONCHAIN, SpEnoteSpentStatus::SPENT_UNCONFIRMED}) == 3);
}
//-------------------------------------------------------------------------------------------------------------------