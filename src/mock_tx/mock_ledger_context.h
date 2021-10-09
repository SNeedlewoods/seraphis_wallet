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

// Mock ledger context: for testing
// NOT FOR PRODUCTION

#pragma once

//local headers
#include "ledger_context.h"
#include "mock_sp_component_types.h"
#include "ringct/rctTypes.h"

//third party headers

//standard headers
#include <unordered_map>
#include <unordered_set>
#include <vector>

//forward declarations


namespace mock_tx
{

class MockLedgerContext final : public LedgerContext
{
public:
    /**
    * brief: linking_tag_exists_sp_v1 - checks if a Seraphis linking tag exists in the ledger
    * param: linking_tag -
    * return: true/false on check result
    */
    bool linking_tag_exists_sp_v1(const rct::key &linking_tag) const override;
    /**
    * brief: get_reference_set_sp_v1 - gets Seraphis enotes stored in the ledger
    * param: indices -
    * outparam: enotes_out - 
    */
    void get_reference_set_sp_v1(const std::vector<std::size_t> &indices,
        std::vector<MockENoteSpV1> &enotes_out) const override;
    /**
    * brief: add_linking_tag_sp_v1 - add a Seraphis linking tag to the ledger
    * param: linking_tag -
    */
    void add_linking_tag_sp_v1(const rct::key &linking_tag);
    /**
    * brief: add_enote_sp_v1 - add a Seraphis v1 enote to the ledger
    * param: enote -
    * return: index in the ledger of the enote just added
    */
    std::size_t add_enote_sp_v1(const MockENoteSpV1 &enote);

private:
    /// Seraphis linking tags
    std::unordered_set<rct::key> m_sp_linking_tags;
    /// Seraphis v1 ENotes
    std::unordered_map<std::size_t, MockENoteSpV1> m_sp_enotes;
};

} //namespace mock_tx
