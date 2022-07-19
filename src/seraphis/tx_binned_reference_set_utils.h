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

// Utilities for interacting with binned reference sets.


#pragma once

//local headers
#include "ringct/rctTypes.h"

//third party headers
#include "tx_binned_reference_set.h"
#include "tx_ref_set_index_mapper.h"

//standard headers
#include <cstdint>
#include <vector>

//forward declarations


namespace sp
{

/**
* brief: check_bin_config_v1 - check that a reference set bin configuration is valid
* param: reference_set_size -
* param: bin_config -
* return: true if the config is valid
*/
bool check_bin_config_v1(const std::uint64_t reference_set_size, const SpBinnedReferenceSetConfigV1 &bin_config);
/**
* brief: make_binned_reference_set_v1 - make a binned reference set (a reference set represented using bins, where one
*    reference element is pre-defined but should be hidden within the generated reference set)
* param: index_mapper -
* param: bin_config -
* param: generator_seed -
* param: reference_set_size -
* param: real_reference_index -
* outparam: binned_reference_set_out -
*/
void make_binned_reference_set_v1(const SpRefSetIndexMapper &index_mapper,
    const SpBinnedReferenceSetConfigV1 &bin_config,
    const rct::key &generator_seed,
    const std::uint64_t reference_set_size,
    const std::uint64_t real_reference_index,
    SpBinnedReferenceSetV1 &binned_reference_set_out);
/**
* brief: try_get_reference_indices_from_binned_reference_set_v1 - try to converted a binned reference set into a list of
*    indices (i.e. the underlying reference set)
* param: binned_reference_set -
* outparam: reference_indices_out -
* return: true if extracting the reference indices succeeded
*/
bool try_get_reference_indices_from_binned_reference_set_v1(const SpBinnedReferenceSetV1 &binned_reference_set,
    std::vector<std::uint64_t> &reference_indices_out);

} //namespace sp