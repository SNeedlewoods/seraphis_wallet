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

// Mock offchain context: for testing


#pragma once

//local headers
#include "crypto/crypto.h"
#include "ringct/rctOps.h"
#include "ringct/rctTypes.h"
#include "tx_component_types.h"

//third party headers
#include <boost/thread/shared_mutex.hpp>

//standard headers
#include <map>
#include <tuple>
#include <unordered_set>
#include <vector>

//forward declarations
namespace sp
{
    struct SpEnoteV1;
    struct SpPartialTxV1;
    struct SpTxSquashedV1;
    struct EnoteScanningChunkLedgerV1;
    struct EnoteScanningChunkNonLedgerV1;
}


namespace sp
{

class MockOffchainContext final
{
public:
    /**
    * brief: key_image_exists_onchain_v1 - checks if a Seraphis linking tag (key image) exists in the ledger
    * param: key_image -
    * return: true/false on check result
    */
    bool key_image_exists_v1(const crypto::key_image &key_image) const;
    /**
    * brief: try_get_offchain_chunk - try to find-received scan the offchain tx cache
    * param: k_find_received -
    * outparam: chunk_out -
    * return: true if chunk is not empty
    */
    bool try_get_offchain_chunk(const crypto::secret_key &k_find_received, EnoteScanningChunkNonLedgerV1 &chunk_out) const;
    /**
    * brief: try_add_partial_tx_v1 - try to add a partial transaction to the offchain tx cache
    *   - fails if there are key image duplicates with: offchain
    * param: partial_tx -
    * return: true if adding succeeded
    */
    bool try_add_partial_tx_v1(const SpPartialTxV1 &partial_tx);
    /**
    * brief: try_add_tx_v1 - try to add a full transaction to the offchain tx cache
    *   - fails if there are key image duplicates with: offchain
    * param: tx -
    * return: true if adding succeeded
    */
    bool try_add_tx_v1(const SpTxSquashedV1 &tx);
    /**
    * brief: remove_tx_from_cache - remove a tx or partial tx from the offchain cache
    * param: input_context - input context of tx/partial tx to remove
    */
    void remove_tx_from_cache(const rct::key &input_context);
    /**
    * brief: remove_tx_with_key_image_from_cache - remove the tx with a specified key image from the offchain cache
    * param: key_image - key image in tx/partial tx to remove
    */
    void remove_tx_with_key_image_from_cache(const crypto::key_image &key_image);
    /**
    * brief: clear_cache - remove all data stored in offchain cache
    */
    void clear_cache();

private:
    /// implementations of the above, without internally locking the ledger mutex (all expected to be no-fail)
    bool key_image_exists_v1_impl(const crypto::key_image &key_image) const;
    bool try_get_offchain_chunk_impl(const crypto::secret_key &k_find_received,
        EnoteScanningChunkNonLedgerV1 &chunk_out) const;
    bool try_add_v1_impl(const std::vector<SpEnoteImageV1> &input_images,
        const SpTxSupplementV1 &tx_supplement,
        const std::vector<SpEnoteV1> &output_enotes);
    bool try_add_partial_tx_v1_impl(const SpPartialTxV1 &partial_tx);
    bool try_add_tx_v1_impl(const SpTxSquashedV1 &tx);
    void remove_tx_from_cache_impl(const rct::key &input_context);
    void remove_tx_with_key_image_from_cache_impl(const crypto::key_image &key_image);
    void clear_cache_impl();

    /// context mutex (mutable for use in const member functions)
    mutable boost::shared_mutex m_context_mutex;

    /// Seraphis key images
    std::unordered_set<crypto::key_image> m_sp_key_images;
    /// map of tx outputs
    std::unordered_map<
        rct::key,     // input context
        std::tuple<       // tx output contents
            SpTxSupplementV1,        // tx supplement
            std::vector<SpEnoteV1>   // output enotes
        >
    > m_output_contents;
    /// map of tx key images
    std::unordered_map<
        rct::key,     // input context
        std::vector<crypto::key_image>  // key images in tx
    > m_tx_key_images;
};

} //namespace sp
