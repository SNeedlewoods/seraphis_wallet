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

// NOT FOR PRODUCTION

//paired header
#include "tx_builders_outputs.h"

//local headers
#include "crypto/crypto.h"
#include "cryptonote_config.h"
#include "seraphis/tx_extra.h"
#include "seraphis_config_temp.h"
#include "jamtis_core_utils.h"
#include "jamtis_destination.h"
#include "jamtis_payment_proposal.h"
#include "jamtis_support_types.h"
#include "misc_log_ex.h"
#include "ringct/rctOps.h"
#include "ringct/rctTypes.h"
#include "tx_builder_types.h"
#include "tx_component_types.h"
#include "tx_extra.h"
#include "tx_misc_utils.h"

//third party headers
#include "boost/multiprecision/cpp_int.hpp"

//standard headers
#include <algorithm>
#include <string>
#include <vector>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "seraphis"

namespace sp
{
//-------------------------------------------------------------------------------------------------------------------
// check that all enote ephemeral pubkeys in an output proposal set are unique
//-------------------------------------------------------------------------------------------------------------------
static bool ephemeral_pubkeys_are_unique_v1(const std::vector<SpOutputProposalV1> &output_proposals)
{
    for (auto output_it = output_proposals.begin(); output_it != output_proposals.end(); ++output_it)
    {
        if (std::find_if(output_proposals.begin(), output_it,
                    [&output_it](const SpOutputProposalV1 &previous_proposal) -> bool
                    {
                        return previous_proposal.m_enote_ephemeral_pubkey == output_it->m_enote_ephemeral_pubkey;
                    }
                ) != output_it)
            return false;
    }

    return true;
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void make_additional_output_normal_self_send_v1(const jamtis::JamtisSelfSendType self_send_type,
    const jamtis::JamtisDestinationV1 &destination,
    const rct::xmr_amount amount,
    jamtis::JamtisPaymentProposalSelfSendV1 &selfsend_proposal_out)
{
    // build payment proposal for a 'normal' self-send
    selfsend_proposal_out.m_destination = destination;
    selfsend_proposal_out.m_amount = amount;
    selfsend_proposal_out.m_type = self_send_type;
    selfsend_proposal_out.m_enote_ephemeral_privkey = rct::rct2sk(rct::skGen());
    selfsend_proposal_out.m_partial_memo = TxExtra{};
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static void make_additional_output_special_self_send_v1(const jamtis::JamtisSelfSendType self_send_type,
    const rct::key &enote_ephemeral_pubkey,
    const jamtis::JamtisDestinationV1 &destination,
    const crypto::secret_key &k_view_balance,
    const rct::xmr_amount amount,
    jamtis::JamtisPaymentProposalSelfSendV1 &selfsend_proposal_out)
{
    // build payment proposal for a 'special' self-send that uses a shared enote ephemeral pubkey
    crypto::secret_key findreceived_key;
    jamtis::make_jamtis_findreceived_key(k_view_balance, findreceived_key);
    const rct::key special_addr_K2{
            rct::scalarmultKey(enote_ephemeral_pubkey, rct::sk2rct(findreceived_key))
        };  //k_fr * K_e_other

    selfsend_proposal_out.m_destination = destination;
    selfsend_proposal_out.m_destination.m_addr_K2 = special_addr_K2;  //k_fr * K_e_other
    selfsend_proposal_out.m_destination.m_addr_K3 = enote_ephemeral_pubkey;  //K_e_other
    selfsend_proposal_out.m_amount = amount;
    selfsend_proposal_out.m_type = self_send_type;
    selfsend_proposal_out.m_enote_ephemeral_privkey = rct::rct2sk(rct::identity());  //r = 1 (not needed)
    selfsend_proposal_out.m_partial_memo = TxExtra{};
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
void check_v1_output_proposal_set_semantics_v1(const std::vector<SpOutputProposalV1> &output_proposals)
{
    CHECK_AND_ASSERT_THROW_MES(output_proposals.size() >= 1, "Semantics check output proposals v1: insufficient outputs.");

    // if 2 proposals, must be a shared enote ephemeral pubkey
    if (output_proposals.size() == 2)
    {
        CHECK_AND_ASSERT_THROW_MES(output_proposals[0].m_enote_ephemeral_pubkey == 
                output_proposals[1].m_enote_ephemeral_pubkey,
            "Semantics check output proposals v1: there are 2 outputs but they don't share an enote ephemeral pubkey.");
    }

    // if >2 proposals, all enote ephemeral pubkeys should be unique
    if (output_proposals.size() > 2)
    {
        for (auto output_it = output_proposals.begin(); output_it != output_proposals.end(); ++output_it)
        {
            CHECK_AND_ASSERT_THROW_MES(ephemeral_pubkeys_are_unique_v1(output_proposals),
                "Semantics check output proposals v1: there are >2 outputs but their enote ephemeral pubkeys aren't all "
                "unique.");
        }
    }

    // proposals should be sorted
    CHECK_AND_ASSERT_THROW_MES(std::is_sorted(output_proposals.begin(), output_proposals.end()),
        "Semantics check output proposals v1: outputs aren't sorted.");

    // proposals should be unique (can use adjacent_find when sorted)
    CHECK_AND_ASSERT_THROW_MES(std::adjacent_find(output_proposals.begin(), output_proposals.end(), equals_from_less{})
            == output_proposals.end(),
        "Semantics check output proposals v1: output onetime addresses are not all unique.");

    // proposal onetime addresses should be canonical (sanity check so our tx outputs don't have duplicate key images)
    for (const SpOutputProposalV1 &output_proposal : output_proposals)
    {
        CHECK_AND_ASSERT_THROW_MES(output_proposal.m_core.onetime_address_is_canonical(),
            "Semantics check output proposals v1: an output onetime address is not in the prime subgroup.");
    }
}
//-------------------------------------------------------------------------------------------------------------------
void check_v1_tx_supplement_semantics_v1(const SpTxSupplementV1 &tx_supplement, const std::size_t num_outputs)
{
    // there may be either 1 or 3+ enote pubkeys
    if (num_outputs == 2)
    {
        CHECK_AND_ASSERT_THROW_MES(tx_supplement.m_output_enote_ephemeral_pubkeys.size() == 1,
            "Semantics check tx supplement v1: there must be 1 enote pubkey if there are 2 outputs.");
    }
    else if (num_outputs >= 3)
    {
        CHECK_AND_ASSERT_THROW_MES(tx_supplement.m_output_enote_ephemeral_pubkeys.size() == num_outputs,
            "Semantics check tx supplement v1: there must be one enote pubkey for each output when there are 3+ outputs.");
    }

    // if 3+ enote pubkeys, all should be unique
    if (tx_supplement.m_output_enote_ephemeral_pubkeys.size() >= 3)
    {
        for (auto enote_pubkey_it = tx_supplement.m_output_enote_ephemeral_pubkeys.begin();
            enote_pubkey_it != tx_supplement.m_output_enote_ephemeral_pubkeys.end();
            ++enote_pubkey_it)
        {
            CHECK_AND_ASSERT_THROW_MES(std::find(tx_supplement.m_output_enote_ephemeral_pubkeys.begin(), enote_pubkey_it,
                    *enote_pubkey_it) == enote_pubkey_it,
                "Semantics check tx supplement v1: enote pubkeys must be unique.");
        }
    }
}
//-------------------------------------------------------------------------------------------------------------------
void make_v1_outputs_v1(const std::vector<SpOutputProposalV1> &output_proposals,
    std::vector<SpEnoteV1> &outputs_out,
    std::vector<rct::xmr_amount> &output_amounts_out,
    std::vector<crypto::secret_key> &output_amount_commitment_blinding_factors_out,
    std::vector<rct::key> &output_enote_ephemeral_pubkeys_out)
{
    outputs_out.clear();
    outputs_out.reserve(output_proposals.size());
    output_amounts_out.clear();
    output_amounts_out.reserve(output_proposals.size());
    output_amount_commitment_blinding_factors_out.clear();
    output_amount_commitment_blinding_factors_out.reserve(output_proposals.size());
    output_enote_ephemeral_pubkeys_out.clear();
    output_enote_ephemeral_pubkeys_out.reserve(output_proposals.size());

    for (const SpOutputProposalV1 &proposal : output_proposals)
    {
        // sanity check
        // note: a blinding factor of 0 is allowed (but not recommended)
        CHECK_AND_ASSERT_THROW_MES(sc_check(to_bytes(proposal.get_amount_blinding_factor())) == 0,
            "making v1 outputs: invalid amount blinding factor (non-canonical).");

        // convert to enote
        outputs_out.emplace_back();
        proposal.get_enote_v1(outputs_out.back());

        // prepare for range proofs
        output_amounts_out.emplace_back(proposal.get_amount());
        output_amount_commitment_blinding_factors_out.emplace_back(proposal.get_amount_blinding_factor());

        // copy non-duplicate enote pubkeys to tx supplement
        if (std::find(output_enote_ephemeral_pubkeys_out.begin(),
            output_enote_ephemeral_pubkeys_out.end(),
            proposal.m_enote_ephemeral_pubkey) == output_enote_ephemeral_pubkeys_out.end())
        {
            output_enote_ephemeral_pubkeys_out.emplace_back(proposal.m_enote_ephemeral_pubkey);
        }
    }
}
//-------------------------------------------------------------------------------------------------------------------
void get_additional_output_types_for_output_set_v1(const rct::key &wallet_spend_pubkey,
    const crypto::secret_key &k_view_balance,
    const std::vector<SpOutputProposalV1> &output_proposals,
    const rct::key &input_context,
    const rct::xmr_amount change_amount,
    OutputProposalSetExtraTypesContextV1 &additional_outputs_context_out,
    std::vector<OutputProposalSetExtraTypesV1> &additional_outputs_out)
{
    if (output_proposals.size() == 0)
    {
        // txs should have at least 1 non-change output

        CHECK_AND_ASSERT_THROW_MES(false, "Finalize output proposals: 0 outputs specified. If you want to send money to "
            "yourself, use a self-spend enote type instead of forcing it via a change enote type.");
    }

    // set the shared enote ephemeral pubkey here: it will always be the first one when it is needed
    additional_outputs_context_out.m_shared_enote_ephemeral_pubkey = output_proposals[0].m_enote_ephemeral_pubkey;

    // add the extra outputs needed
    additional_outputs_out.clear();
    additional_outputs_out.reserve(2);

    if (output_proposals.size() == 1)
    {
        // if the output is a self-send, get its type
        jamtis::JamtisSelfSendType single_self_send_type;

        const bool single_is_self_send{
                jamtis::try_get_self_send_type(output_proposals[0],
                    input_context,
                    wallet_spend_pubkey,
                    k_view_balance,
                    single_self_send_type)
                };

        if (change_amount == 0)
        {
            if (single_is_self_send)
            {
                // txs need at least 2 outputs; we already have a self-send, so make a random special dummy output

                // add a special dummy output
                // - 0 amount
                // - make sure the final proposal set will have 1 unique enote ephemeral pubkey
                additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::SPECIAL_DUMMY);
            }
            else //(no self-send)
            {
                // txs need at least 2 outputs, with at least 1 self-send enote type

                // add a special self-send dummy output
                // - 0 amount
                // - make sure the final proposal set will have 1 unique enote ephemeral pubkey
                additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::SPECIAL_SELF_SEND_DUMMY);
            }
        }
        else if (/*change_amount > 0 &&*/
            single_is_self_send &&
            single_self_send_type == jamtis::JamtisSelfSendType::CHANGE)
        {
            // 2-out txs may not have 2 self-send type enotes of the same type from the same wallet, so since
            //   we already have a change output (for some dubious reason) we can't have a special change here
            // reason: the outputs in a 2-out tx with 2 same-type self-sends would have the same sender-receiver shared
            //         secret, which could cause problems (e.g. the outputs would have the same view tags, and could even
            //         have the same onetime address if the destinations of the two outputs are the same)

            // two change outputs doesn't make sense, so just ban it
            CHECK_AND_ASSERT_THROW_MES(false, "Finalize output proposals: there is 1 change-type output already specified, "
                "but the change amount is non-zero and a tx with just two change outputs is not allowed for privacy reasons. "
                "If you want to make a tx with just two change outputs, avoid calling this function (not recommended).");
        }
        else //(change_amount > 0 && single output is not a self-send change)
        {
            // if there is 1 non-change output and non-zero change, then make a special change enote that shares
            //   the other output's enote ephemeral pubkey

            // add a special change output
            // - 'change' amount
            // - make sure the final proposal set will have 1 unique enote ephemeral pubkey
            additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::SPECIAL_CHANGE);
        }
    }
    else if (output_proposals.size() == 2 && ephemeral_pubkeys_are_unique_v1(output_proposals))
    {
        if (change_amount == 0)
        {
            // 2-out txs need 1 shared enote ephemeral pubkey; add a dummy output here since the outputs have different
            //   enote ephemeral pubkeys

            if (jamtis::is_self_send_output_proposal(output_proposals[0],
                    input_context,
                    wallet_spend_pubkey,
                    k_view_balance) ||
                jamtis::is_self_send_output_proposal(output_proposals[1],
                    input_context,
                    wallet_spend_pubkey,
                    k_view_balance))
            {
                // if we have at least 1 self-send already, we can just make a normal dummy output

                // add a normal dummy output
                // - 0 amount
                additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::NORMAL_DUMMY);
            }
            else //(no self-sends)
            {
                // if there are no self-sends, then we need to add a dummy self-send

                // add a normal self-send dummy output
                // - 0 amount
                additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::NORMAL_SELF_SEND_DUMMY);
            }
        }
        else //(change_amount > 0)
        {
            // 2 separate outputs + 1 change output = a simple 3-out tx

            // add a normal change output
            // - 'change' amount
            additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::NORMAL_CHANGE);
        }
    }
    else if (output_proposals.size() == 2 && !ephemeral_pubkeys_are_unique_v1(output_proposals))
    {
        if (change_amount == 0)
        {
            // if the two outputs are self-sends, get their types
            jamtis::JamtisSelfSendType first_self_send_type;
            jamtis::JamtisSelfSendType second_self_send_type;

            const bool first_is_self_send{
                    jamtis::try_get_self_send_type(output_proposals[0],
                        input_context,
                        wallet_spend_pubkey,
                        k_view_balance,
                        first_self_send_type)
                };
            const bool second_is_self_send{
                    jamtis::try_get_self_send_type(output_proposals[1],
                        input_context,
                        wallet_spend_pubkey,
                        k_view_balance,
                        second_self_send_type)
                };

            if (first_is_self_send && second_is_self_send && first_self_send_type == second_self_send_type)
            {
                CHECK_AND_ASSERT_THROW_MES(false, "Finalize output proposals: there are 2 self-send outputs with the same "
                    "type that share an enote ephemeral pubkey, but this can reduce user privacy. If you want to send "
                    "money to yourself, make independent self-spend types, or avoid calling this function (not recommended).");
            }
            else if (first_is_self_send || second_is_self_send)
            {
                // do nothing: the proposal set is already 'final'
            }
            else //(no self-sends)
            {
                CHECK_AND_ASSERT_THROW_MES(false, "Finalize output proposals: there are 2 normal outputs that share "
                    "an enote ephemeral pubkey, but every normally-constructed tx needs at least one self-send output (since "
                    "the 2 outputs share an enote ephemeral pubkey, we can't add a dummy self-send). If you want to make a "
                    "2-output tx with no self-sends, then avoid calling this function (not recommended without good reason).");
            }
        }
        else //(change_amount > 0)
        {
            CHECK_AND_ASSERT_THROW_MES(false, "Finalize output proposals: there are 2 outputs that share "
                "an enote ephemeral pubkey, but a non-zero change amount. In >2-out txs, all enote ephemeral pubkeys should "
                "be unique, so adding a change output isn't feasible here. You need to make independent output proposals, "
                "or avoid calling this function (not recommended).");
        }
    }
    else //(output_proposals.size() > 2)
    {
        CHECK_AND_ASSERT_THROW_MES(ephemeral_pubkeys_are_unique_v1(output_proposals),
            "Finalize output proposals: there are >2 outputs but their enote ephemeral pubkeys aren't all unique.");

        if (change_amount == 0)
        {
            // see if there is a self-send proposal
            bool has_self_send{false};

            for (const SpOutputProposalV1 &output_proposal : output_proposals)
            {
                if (jamtis::is_self_send_output_proposal(output_proposal,
                    input_context,
                    wallet_spend_pubkey,
                    k_view_balance))
                {
                    has_self_send = true;
                    break;
                }
            }

            if (has_self_send)
            {
                // do nothing: the proposal set is already 'final'
            }
            else //(no self-sends)
            {
                // every tx made by this function needs a self-send output, so make a dummy self-send here

                // add a normal self-send dummy output
                // - 0 amount
                additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::NORMAL_SELF_SEND_DUMMY);
            }
        }
        else //(change_amount > 0)
        {
            // >2 separate outputs + 1 change output = a simple tx with 3+ outputs

            // add a normal change output
            // - 'change' amount
            additional_outputs_out.emplace_back(OutputProposalSetExtraTypesV1::NORMAL_CHANGE);
        }
    }
}
//-------------------------------------------------------------------------------------------------------------------
void make_additional_output_dummy_v1(const OutputProposalSetExtraTypesV1 additional_output_type,
    const OutputProposalSetExtraTypesContextV1 &additional_outputs_context,
    SpOutputProposalV1 &output_proposal_out)
{
    // choose which output type to make, and make it
    if (additional_output_type == OutputProposalSetExtraTypesV1::NORMAL_DUMMY)
    {
        // normal dummy
        // - 0 amount
        output_proposal_out.gen(0, 0);
    }
    else if (additional_output_type == OutputProposalSetExtraTypesV1::SPECIAL_DUMMY)
    {
        // special dummy
        // - 0 amount
        // - shared enote ephemeral pubkey
        output_proposal_out.gen(0, 0);
        output_proposal_out.m_enote_ephemeral_pubkey = additional_outputs_context.m_shared_enote_ephemeral_pubkey;
    }
    else
    {
        CHECK_AND_ASSERT_THROW_MES(false, "Unknown output proposal set extra type (dummy).");
    }
}
//-------------------------------------------------------------------------------------------------------------------
void make_additional_output_selfsend_v1(const OutputProposalSetExtraTypesV1 additional_output_type,
    const OutputProposalSetExtraTypesContextV1 &additional_outputs_context,
    const jamtis::JamtisDestinationV1 &change_destination,
    const jamtis::JamtisDestinationV1 &dummy_destination,
    const crypto::secret_key &k_view_balance,
    const rct::xmr_amount change_amount,
    jamtis::JamtisPaymentProposalSelfSendV1 &selfsend_proposal_out)
{
    // choose which output type to make, and make it
    if (additional_output_type == OutputProposalSetExtraTypesV1::NORMAL_SELF_SEND_DUMMY)
    {
        // normal self-send dummy
        // - 0 amount
        make_additional_output_normal_self_send_v1(jamtis::JamtisSelfSendType::DUMMY,
            dummy_destination,
            0,
            selfsend_proposal_out);
    }
    else if (additional_output_type == OutputProposalSetExtraTypesV1::NORMAL_CHANGE)
    {
        // normal change
        // - 'change' amount
        make_additional_output_normal_self_send_v1(jamtis::JamtisSelfSendType::CHANGE,
            change_destination,
            change_amount,
            selfsend_proposal_out);
    }
    else if (additional_output_type == OutputProposalSetExtraTypesV1::SPECIAL_SELF_SEND_DUMMY)
    {
        // special self-send dummy
        // - 0 amount
        // - shared enote ephemeral pubkey
        make_additional_output_special_self_send_v1(jamtis::JamtisSelfSendType::DUMMY,
            additional_outputs_context.m_shared_enote_ephemeral_pubkey,
            dummy_destination,
            k_view_balance,
            0,
            selfsend_proposal_out);
        
    }
    else if (additional_output_type == OutputProposalSetExtraTypesV1::SPECIAL_CHANGE)
    {
        // special change
        // - 'change' amount
        // - shared enote ephemeral pubkey
        make_additional_output_special_self_send_v1(jamtis::JamtisSelfSendType::CHANGE,
            additional_outputs_context.m_shared_enote_ephemeral_pubkey,
            change_destination,
            k_view_balance,
            change_amount,
            selfsend_proposal_out);
    }
    else
    {
        CHECK_AND_ASSERT_THROW_MES(false, "Unknown output proposal set extra type (self-send).");
    }
}
//-------------------------------------------------------------------------------------------------------------------
void finalize_v1_output_proposal_set_v1(const boost::multiprecision::uint128_t &total_input_amount,
    const rct::xmr_amount transaction_fee,
    const jamtis::JamtisDestinationV1 &change_destination,
    const jamtis::JamtisDestinationV1 &dummy_destination,
    const rct::key &input_context,
    const rct::key &wallet_spend_pubkey,
    const crypto::secret_key &k_view_balance,
    const std::vector<SpOutputProposalV1> &original_output_proposals,
    std::vector<SpOutputProposalV1> &new_output_proposals_out,
    std::vector<jamtis::JamtisPaymentProposalSelfSendV1> &new_selfsend_proposals_out)
{
    // get change amount
    boost::multiprecision::uint128_t output_sum{transaction_fee};

    for (const SpOutputProposalV1 &proposal : original_output_proposals)
        output_sum += proposal.get_amount();

    CHECK_AND_ASSERT_THROW_MES(total_input_amount >= output_sum, "Finalize output proposals: input amount is too small.");
    CHECK_AND_ASSERT_THROW_MES(total_input_amount - output_sum <= static_cast<rct::xmr_amount>(-1),
        "Finalize output proposals: change amount exceeds maximum value allowed.");

    const rct::xmr_amount change_amount{total_input_amount - output_sum};

    // get output types to add
    OutputProposalSetExtraTypesContextV1 additional_outputs_context;
    std::vector<OutputProposalSetExtraTypesV1> additional_outputs;

    get_additional_output_types_for_output_set_v1(wallet_spend_pubkey,
        k_view_balance,
        original_output_proposals,
        input_context,
        change_amount,
        additional_outputs_context,
        additional_outputs);

    // add the new outputs
    new_output_proposals_out.clear();
    new_selfsend_proposals_out.clear();

    for (const OutputProposalSetExtraTypesV1 additional_output_type : additional_outputs)
    {
        if (additional_output_type == OutputProposalSetExtraTypesV1::NORMAL_DUMMY ||
            additional_output_type == OutputProposalSetExtraTypesV1::SPECIAL_DUMMY)
        {
            new_output_proposals_out.emplace_back();
            make_additional_output_dummy_v1(additional_output_type,
                additional_outputs_context,
                new_output_proposals_out.back());
        }
        else
        {
            new_selfsend_proposals_out.emplace_back();
            make_additional_output_selfsend_v1(additional_output_type,
                additional_outputs_context,
                change_destination,
                dummy_destination,
                k_view_balance,
                change_amount,
                new_selfsend_proposals_out.back());
        }
    }
}
//-------------------------------------------------------------------------------------------------------------------
void finalize_v1_output_proposal_set_v1(const boost::multiprecision::uint128_t &total_input_amount,
    const rct::xmr_amount transaction_fee,
    const jamtis::JamtisDestinationV1 &change_destination,
    const jamtis::JamtisDestinationV1 &dummy_destination,
    const rct::key &input_context,
    const rct::key &wallet_spend_pubkey,
    const crypto::secret_key &k_view_balance,
    std::vector<SpOutputProposalV1> &output_proposals_inout)
{
    // make new output and selfsend proposals, then combine into full output set
    std::vector<SpOutputProposalV1> new_output_proposals;
    std::vector<jamtis::JamtisPaymentProposalSelfSendV1> new_selfsend_proposals;

    finalize_v1_output_proposal_set_v1(total_input_amount,
        transaction_fee,
        change_destination,
        dummy_destination,
        input_context,
        wallet_spend_pubkey,
        k_view_balance,
        output_proposals_inout,
        new_output_proposals,
        new_selfsend_proposals);

    output_proposals_inout.reserve(output_proposals_inout.size() +
        new_output_proposals.size() +
        new_selfsend_proposals.size());

    for (SpOutputProposalV1 &output_proposal : new_output_proposals)
        output_proposals_inout.emplace_back(std::move(output_proposal));

    for (const jamtis::JamtisPaymentProposalSelfSendV1 &selfsend_proposal : new_selfsend_proposals)
    {
        output_proposals_inout.emplace_back();
        selfsend_proposal.get_output_proposal_v1(k_view_balance, input_context, output_proposals_inout.back());
    }
}
//-------------------------------------------------------------------------------------------------------------------
void check_v1_tx_proposal_semantics_v1(const SpTxProposalV1 &tx_proposal)
{
    // outputs should be sorted
    CHECK_AND_ASSERT_THROW_MES(std::is_sorted(tx_proposal.m_outputs.begin(), tx_proposal.m_outputs.end()),
        "Semantics check tx proposal v1: outputs aren't sorted.");

    // outputs should be unique (can use adjacent_find when sorted)
    CHECK_AND_ASSERT_THROW_MES(std::adjacent_find(tx_proposal.m_outputs.begin(),
            tx_proposal.m_outputs.end(),
            equals_from_less{}) == tx_proposal.m_outputs.end(),
        "Semantics check tx proposal v1: output onetime addresses are not all unique.");

    // onetime addresses should be canonical (sanity check so our tx outputs don't have duplicate key images)
    for (const SpEnoteV1 &output_enote : tx_proposal.m_outputs)
    {
        CHECK_AND_ASSERT_THROW_MES(output_enote.m_core.onetime_address_is_canonical(),
            "Semantics check tx proposal v1: an output onetime address is not in the prime subgroup.");
    }

    // check that output amount commitments can be reproduced
    CHECK_AND_ASSERT_THROW_MES(tx_proposal.m_outputs.size() == tx_proposal.m_output_amounts.size(),
        "Semantics check tx proposal v1: outputs don't line up with output amounts.");
    CHECK_AND_ASSERT_THROW_MES(tx_proposal.m_outputs.size() ==
            tx_proposal.m_output_amount_commitment_blinding_factors.size(),
        "Semantics check tx proposal v1: outputs don't line up with output amount commitment blinding factors.");

    for (std::size_t output_index{0}; output_index < tx_proposal.m_outputs.size(); ++output_index)
    {
        CHECK_AND_ASSERT_THROW_MES(tx_proposal.m_outputs[output_index].m_core.m_amount_commitment ==
                rct::commit(tx_proposal.m_output_amounts[output_index],
                    rct::sk2rct(tx_proposal.m_output_amount_commitment_blinding_factors[output_index])),
            "Semantics check tx proposal v1: could not reproduce an output's amount commitment.");
    }

    // check tx supplement (especially enote ephemeral pubkeys)
    check_v1_tx_supplement_semantics_v1(tx_proposal.m_tx_supplement, tx_proposal.m_outputs.size());
}
//-------------------------------------------------------------------------------------------------------------------
void make_v1_tx_proposal_v1(std::vector<SpOutputProposalV1> output_proposals,
    std::vector<ExtraFieldElement> additional_memo_elements,
    SpTxProposalV1 &proposal_out)
{
    // outputs should be sorted by onetime address
    std::sort(output_proposals.begin(), output_proposals.end());

    // sanity-check semantics
    check_v1_output_proposal_set_semantics_v1(output_proposals);

    // make outputs
    // make tx supplement
    // prepare for range proofs
    make_v1_outputs_v1(output_proposals,
        proposal_out.m_outputs,
        proposal_out.m_output_amounts,
        proposal_out.m_output_amount_commitment_blinding_factors,
        proposal_out.m_tx_supplement.m_output_enote_ephemeral_pubkeys);

    // add all memo fields to the tx supplement
    for (const SpOutputProposalV1 &output_proposal : output_proposals)
        accumulate_extra_field_elements(output_proposal.m_partial_memo, additional_memo_elements);

    make_tx_extra(std::move(additional_memo_elements), proposal_out.m_tx_supplement.m_tx_extra);

    // sanity-check supplement semantics
    check_v1_tx_supplement_semantics_v1(proposal_out.m_tx_supplement, proposal_out.m_outputs.size());
}
//-------------------------------------------------------------------------------------------------------------------
std::vector<SpOutputProposalV1> gen_mock_sp_output_proposals_v1(const std::vector<rct::xmr_amount> &out_amounts,
    const std::size_t num_random_memo_elements)
{
    // generate random output proposals
    std::vector<SpOutputProposalV1> output_proposals;
    output_proposals.reserve(out_amounts.size());

    for (const rct::xmr_amount out_amount : out_amounts)
    {
        output_proposals.emplace_back();
        output_proposals.back().gen(out_amount, num_random_memo_elements);
    }

    // sort them
    std::sort(output_proposals.begin(), output_proposals.end());

    return output_proposals;
}
//-------------------------------------------------------------------------------------------------------------------
} //namespace sp
