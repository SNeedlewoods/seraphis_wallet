// Copyright (c) 2014-2023, The Monero Project
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
#include "utils.h"

//local headers
#include "common_defines.h"
#include "common/util.h"
#include "crypto/hash.h"
#include "crypto/hash-ops.h"
#include "include_base_utils.h"                     // LOG_PRINT_x

//third party headers
#include "file_io_utils.h"
#include "mlocker.h"
#include "openssl/pem.h"

//standard headers


static const std::string ASCII_OUTPUT_MAGIC = "MoneroAsciiDataV1";


namespace Monero {
namespace Utils {

bool isAddressLocal(const std::string &address)
{ 
    try {
        return tools::is_local_address(address);
    } catch (const std::exception &e) {
        MERROR("error: " << e.what());
        return false;
    }
}

void onStartup()
{
    tools::on_startup();
#ifdef NDEBUG
    tools::disable_core_dumps();
#endif
}

bool save_to_file(const std::string &path_to_file, const std::string &raw, bool is_printable, ExportFormat export_format)
{
    if (is_printable || export_format == ExportFormat::Binary)
        return epee::file_io_utils::save_string_to_file(path_to_file, raw);

    FILE *fp = fopen(path_to_file.c_str(), "w+");
    if (!fp)
    {
        MERROR("Failed to open wallet file for writing: " << path_to_file << ": " << strerror(errno));
        return false;
    }

    // Save the result b/c we need to close the fp before returning success/failure.
    int write_result = PEM_write(fp, ASCII_OUTPUT_MAGIC.c_str(), "", (const unsigned char *) raw.c_str(), raw.length());
    fclose(fp);

    if (write_result == 0)
        return false;
    else
        return true;
}

// TODO : consider using another namespace KeyUtils if more key related stuff ends up here
crypto::chacha_key derive_cache_key(const crypto::chacha_key &keys_data_key)
{
    static_assert(crypto::HASH_SIZE == sizeof(crypto::chacha_key), "Mismatched sizes of hash and chacha key");

    crypto::chacha_key cache_key{};
    epee::mlocked<tools::scrubbed_arr<char, crypto::HASH_SIZE+1>> cache_key_data;
    memcpy(cache_key_data.data(), &keys_data_key, crypto::HASH_SIZE);
    cache_key_data[crypto::HASH_SIZE] = config::HASH_KEY_WALLET_CACHE;
    cn_fast_hash(cache_key_data.data(), crypto::HASH_SIZE+1, (crypto::hash&) cache_key);

    return cache_key;
}

} // namespace Utils
} // namespace Monero
