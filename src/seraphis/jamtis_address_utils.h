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

// Core implementation details for making Jamtis privkeys
// - Jamtis is a specification for Seraphis-compatible addresses


#pragma once

//local headers
extern "C"
{
#include "crypto/crypto-ops.h"
}
#include "device/device.hpp"
#include "ringct/rctTypes.h"

//third party headers

//standard headers
#include <vector>

//forward declarations


namespace sp
{
namespace jamtis
{

/// index (system-endian; only 56 bits are used): j
using address_index_t = std::uint64_t;
constexpr std::size_t ADDRESS_INDEX_BYTES{7};
constexpr address_index_t ADDRESS_INDEX_MAX{(address_index_t{1} << 8*ADDRESS_INDEX_BYTES) - 1};  //2^56 - 1

/// t_view (TODO: core_types file?)
using view_tag_t = unsigned char;

/**
* brief: make_jamtis_findreceived_key - find-received key, for finding enotes received by the wallet
*   - use to compute view tags and nominal spend keys
*   k_fr = H_n(k_vb)
* param: k_view_balance - k_vb
* outparam: ciphertag_secret_out - s_ct
*/
void make_jamtis_findreceived_key(const crypto::secret_key &k_view_balance,
    crypto::secret_key &findreceived_key_out);
/**
* brief: make_jamtis_generateaddress_secret - generate-address secret, for generating addresses
*   s_ga = H_32(k_vb)
* param: k_view_balance - k_vb
* outparam: generateaddress_secret_out - s_ga
*/
void make_jamtis_generateaddress_secret(const crypto::secret_key &k_view_balance,
    crypto::secret_key &generateaddress_secret_out);
/**
* brief: make_jamtis_ciphertag_secret - cipher-tag secret, for ciphering address indices to/from address tags
*   s_ct = H_32(k_ga)
* param: k_generate_address - k_ga
* outparam: ciphertag_secret_out - s_ct
*/
void make_jamtis_ciphertag_secret(const crypto::secret_key &k_generate_address,
    crypto::secret_key &ciphertag_secret_out);
/**
* brief: make_jamtis_identifywallet_key - identify-wallet key, for certifying that an address belongs to a certain wallet
*   k_id = H_n(k_ga)
* param: k_generate_address - k_ga
* outparam: identifywallet_key_out - k_id
*/
void make_jamtis_identifywallet_key(const crypto::secret_key &k_generate_address,
    crypto::secret_key &identifywallet_key_out);

} //namespace jamtis
} //namespace sp
