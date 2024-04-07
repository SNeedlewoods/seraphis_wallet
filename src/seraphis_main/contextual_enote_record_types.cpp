// Copyright (c) 2022, The Monero Project
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

//paired header
#include "contextual_enote_record_types.h"

//local headers
#include "common/variant.h"
#include "crypto/crypto.h"
#include "ringct/rctTypes.h"

//third party headers

//standard headers
#include <algorithm>


#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "seraphis"

namespace sp
{

//-------------------------------------------------------------------------------------------------------------------
std::uint64_t block_index_ref(const LegacyEnoteOriginContextVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<std::uint64_t>
    {
        using variant_static_visitor::operator();  //for blank overload
        std::uint64_t operator()(const LegacyEnoteOriginContextV1 &record) const
        { return record.block_index; }
        std::uint64_t operator()(const LegacyEnoteOriginContextV2 &record) const
        { return record.block_index; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t block_timestamp_ref(const LegacyEnoteOriginContextVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<std::uint64_t>
    {
        using variant_static_visitor::operator();  //for blank overload
        std::uint64_t operator()(const LegacyEnoteOriginContextV1 &record) const
        { return record.block_timestamp; }
        std::uint64_t operator()(const LegacyEnoteOriginContextV2 &record) const
        { return record.block_timestamp; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const rct::key& transaction_id_ref(const LegacyEnoteOriginContextVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const rct::key&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const rct::key& operator()(const LegacyEnoteOriginContextV1 &record) const
        { return record.transaction_id; }
        const rct::key& operator()(const LegacyEnoteOriginContextV2 &record) const
        { return record.transaction_id; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t enote_ledger_index_ref(const LegacyEnoteOriginContextVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<std::uint64_t>
    {
        using variant_static_visitor::operator();  //for blank overload
        std::uint64_t operator()(const LegacyEnoteOriginContextV1 &record) const
        { return record.enote_ledger_index; }
        std::uint64_t operator()(const LegacyEnoteOriginContextV2 &record) const
        { return record.enote_ledger_index; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
SpEnoteOriginStatus origin_status_ref(const LegacyEnoteOriginContextVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<SpEnoteOriginStatus>
    {
        using variant_static_visitor::operator();  //for blank overload
        SpEnoteOriginStatus operator()(const LegacyEnoteOriginContextV1 &record) const
        { return record.origin_status; }
        SpEnoteOriginStatus operator()(const LegacyEnoteOriginContextV2 &record) const
        { return record.origin_status; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t enote_version_dependent_index_ref(const LegacyEnoteOriginContextVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<std::uint64_t>
    {
        using variant_static_visitor::operator();  //for blank overload
        std::uint64_t operator()(const LegacyEnoteOriginContextV1 &record) const
        { return record.enote_same_amount_ledger_index; }
        std::uint64_t operator()(const LegacyEnoteOriginContextV2 &record) const
        { return record.rct_enote_ledger_index; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const rct::key& onetime_address_ref(const LegacyContextualIntermediateEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const rct::key&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const rct::key& operator()(const LegacyContextualIntermediateEnoteRecordV1 &record) const
        { return onetime_address_ref(record.record.enote); }
        const rct::key& operator()(const LegacyContextualIntermediateEnoteRecordV2 &record) const
        { return onetime_address_ref(record.record.enote); }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
rct::xmr_amount amount_ref(const LegacyContextualIntermediateEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<rct::xmr_amount>
    {
        using variant_static_visitor::operator();  //for blank overload
        rct::xmr_amount operator()(const LegacyContextualIntermediateEnoteRecordV1 &record) const
        { return record.record.amount; }
        rct::xmr_amount operator()(const LegacyContextualIntermediateEnoteRecordV2 &record) const
        { return record.record.amount; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t block_index_ref(const LegacyContextualIntermediateEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<std::uint64_t>
    {
        using variant_static_visitor::operator();  //for blank overload
        std::uint64_t operator()(const LegacyContextualIntermediateEnoteRecordV1 &record) const
        { return record.origin_context.block_index; }
        std::uint64_t operator()(const LegacyContextualIntermediateEnoteRecordV2 &record) const
        { return record.origin_context.block_index; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
LegacyIntermediateEnoteRecord enote_record_ref(const LegacyContextualIntermediateEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<LegacyIntermediateEnoteRecord>
    {
        using variant_static_visitor::operator();  //for blank overload
        LegacyIntermediateEnoteRecord operator()(const LegacyContextualIntermediateEnoteRecordV1 &record) const
        { return record.record; }
        LegacyIntermediateEnoteRecord operator()(const LegacyContextualIntermediateEnoteRecordV2 &record) const
        { return record.record; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const SpEnoteOriginStatus& origin_status_ref(const LegacyContextualIntermediateEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const SpEnoteOriginStatus&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const SpEnoteOriginStatus& operator()(const LegacyContextualIntermediateEnoteRecordV1 &record) const
        { return record.origin_context.origin_status; }
        const SpEnoteOriginStatus& operator()(const LegacyContextualIntermediateEnoteRecordV2 &record) const
        { return record.origin_context.origin_status; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const crypto::key_image& key_image_ref(const LegacyContextualEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const crypto::key_image&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const crypto::key_image& operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.record.key_image; }
        const crypto::key_image& operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.record.key_image; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
rct::xmr_amount amount_ref(const LegacyContextualEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<rct::xmr_amount>
    {
        using variant_static_visitor::operator();  //for blank overload
        rct::xmr_amount operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.record.amount; }
        rct::xmr_amount operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.record.amount; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t block_index_ref(const LegacyContextualEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<std::uint64_t>
    {
        using variant_static_visitor::operator();  //for blank overload
        std::uint64_t operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.origin_context.block_index; }
        std::uint64_t operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.origin_context.block_index; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const LegacyEnoteRecord& enote_record_ref(const LegacyContextualEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const LegacyEnoteRecord&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const LegacyEnoteRecord& operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.record; }
        const LegacyEnoteRecord& operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.record; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const SpEnoteOriginStatus& origin_status_ref(const LegacyContextualEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const SpEnoteOriginStatus&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const SpEnoteOriginStatus& operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.origin_context.origin_status; }
        const SpEnoteOriginStatus& operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.origin_context.origin_status; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
LegacyEnoteOriginContextVariant origin_context_ref(const LegacyContextualEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<LegacyEnoteOriginContextVariant>
    {
        using variant_static_visitor::operator();  //for blank overload
        LegacyEnoteOriginContextVariant operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.origin_context; }
        LegacyEnoteOriginContextVariant operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.origin_context; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const SpEnoteSpentContextV1& spent_context_ref(const LegacyContextualEnoteRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const SpEnoteSpentContextV1&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const SpEnoteSpentContextV1& operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.spent_context; }
        const SpEnoteSpentContextV1& operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.spent_context; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
void clear_spent_context(LegacyContextualEnoteRecordVariant &variant)
{
    if (variant.is_type<LegacyContextualEnoteRecordV1>())
        variant.unwrap<LegacyContextualEnoteRecordV1>().spent_context = SpEnoteSpentContextV1{};
    else if (variant.is_type<LegacyContextualEnoteRecordV2>())
        variant.unwrap<LegacyContextualEnoteRecordV2>().spent_context = SpEnoteSpentContextV1{};
    else
        CHECK_AND_ASSERT_THROW_MES(false, "LegacyContextualEnoteRecord version not implemented");
}
//-------------------------------------------------------------------------------------------------------------------
const rct::key& onetime_address_ref(const SpContextualIntermediateEnoteRecordV1 &record)
{
    return onetime_address_ref(record.record.enote);
}
//-------------------------------------------------------------------------------------------------------------------
rct::xmr_amount amount_ref(const SpContextualIntermediateEnoteRecordV1 &record)
{
    return record.record.amount;
}
//-------------------------------------------------------------------------------------------------------------------
const crypto::key_image& key_image_ref(const SpContextualEnoteRecordV1 &record)
{
    return record.record.key_image;
}
//-------------------------------------------------------------------------------------------------------------------
rct::xmr_amount amount_ref(const SpContextualEnoteRecordV1 &record)
{
    return record.record.amount;
}
//-------------------------------------------------------------------------------------------------------------------
SpEnoteOriginStatus origin_status_ref(const ContextualBasicRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<SpEnoteOriginStatus>
    {
        using variant_static_visitor::operator();  //for blank overload
        SpEnoteOriginStatus operator()(const LegacyContextualBasicEnoteRecordV1 &record) const
        { return origin_status_ref(record.origin_context); }
        SpEnoteOriginStatus operator()(const SpContextualBasicEnoteRecordV1 &record) const
        { return record.origin_context.origin_status; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const rct::key& transaction_id_ref(const ContextualBasicRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const rct::key&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const rct::key& operator()(const LegacyContextualBasicEnoteRecordV1 &record) const
        { return transaction_id_ref(record.origin_context); }
        const rct::key& operator()(const SpContextualBasicEnoteRecordV1 &record) const
        { return record.origin_context.transaction_id; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
std::uint64_t block_index_ref(const ContextualBasicRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const std::uint64_t>
    {
        using variant_static_visitor::operator();  //for blank overload
        std::uint64_t operator()(const LegacyContextualBasicEnoteRecordV1 &record) const
        { return block_index_ref(record.origin_context); }
        std::uint64_t operator()(const SpContextualBasicEnoteRecordV1 &record) const
        { return record.origin_context.block_index; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
rct::xmr_amount amount_ref(const ContextualRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<rct::xmr_amount>
    {
        using variant_static_visitor::operator();  //for blank overload
        rct::xmr_amount operator()(const LegacyContextualEnoteRecordV1 &record) const { return record.record.amount; }
        rct::xmr_amount operator()(const LegacyContextualEnoteRecordV2 &record) const { return record.record.amount; }
        rct::xmr_amount operator()(const SpContextualEnoteRecordV1 &record)     const { return amount_ref(record); }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
const SpEnoteSpentContextV1& spent_context_ref(const ContextualRecordVariant &variant)
{
    struct visitor final : public tools::variant_static_visitor<const SpEnoteSpentContextV1&>
    {
        using variant_static_visitor::operator();  //for blank overload
        const SpEnoteSpentContextV1& operator()(const LegacyContextualEnoteRecordV1 &record) const
        { return record.spent_context; }
        const SpEnoteSpentContextV1& operator()(const LegacyContextualEnoteRecordV2 &record) const
        { return record.spent_context; }
        const SpEnoteSpentContextV1& operator()(const SpContextualEnoteRecordV1 &record) const
        { return record.spent_context; }
    };

    return variant.visit(visitor());
}
//-------------------------------------------------------------------------------------------------------------------
bool is_older_than(const LegacyEnoteOriginContextV1 &context, const LegacyEnoteOriginContextV1 &other_context)
{
    // 1. origin status (higher statuses are assumed to be 'older')
    if (context.origin_status > other_context.origin_status)
        return true;
    if (context.origin_status < other_context.origin_status)
        return false;

    // 2. block index
    if (context.block_index < other_context.block_index)
        return true;
    if (context.block_index > other_context.block_index)
        return false;

    // note: don't assess the tx output index

    // 3. enote ledger index
    if (context.enote_ledger_index < other_context.enote_ledger_index)
        return true;
    if (context.enote_ledger_index > other_context.enote_ledger_index)
        return false;

    // 4. block timestamp
    if (context.block_timestamp < other_context.block_timestamp)
        return true;
    if (context.block_timestamp > other_context.block_timestamp)
        return false;

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool is_older_than(const LegacyEnoteOriginContextV2 &context, const LegacyEnoteOriginContextV2 &other_context)
{
    // 1. origin status (higher statuses are assumed to be 'older')
    if (context.origin_status > other_context.origin_status)
        return true;
    if (context.origin_status < other_context.origin_status)
        return false;

    // 2. block index
    if (context.block_index < other_context.block_index)
        return true;
    if (context.block_index > other_context.block_index)
        return false;

    // note: don't assess the tx output index

    // 3. enote zero amount ledger index
    if (context.rct_enote_ledger_index < other_context.rct_enote_ledger_index)
        return true;
    if (context.rct_enote_ledger_index > other_context.rct_enote_ledger_index)
        return false;

    // 4. enote ledger index
    if (context.enote_ledger_index < other_context.enote_ledger_index)
        return true;
    if (context.enote_ledger_index > other_context.enote_ledger_index)
        return false;

    // 5. block timestamp
    if (context.block_timestamp < other_context.block_timestamp)
        return true;
    if (context.block_timestamp > other_context.block_timestamp)
        return false;

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool is_older_than(const SpEnoteOriginContextV1 &context, const SpEnoteOriginContextV1 &other_context)
{
    // 1. origin status (higher statuses are assumed to be 'older')
    if (context.origin_status > other_context.origin_status)
        return true;
    if (context.origin_status < other_context.origin_status)
        return false;

    // 2. block index
    if (context.block_index < other_context.block_index)
        return true;
    if (context.block_index > other_context.block_index)
        return false;

    // note: don't assess the tx output index

    // 3. enote ledger index
    if (context.enote_ledger_index < other_context.enote_ledger_index)
        return true;
    if (context.enote_ledger_index > other_context.enote_ledger_index)
        return false;

    // 4. block timestamp
    if (context.block_timestamp < other_context.block_timestamp)
        return true;
    if (context.block_timestamp > other_context.block_timestamp)
        return false;

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool is_older_than(const SpEnoteSpentContextV1 &context, const SpEnoteSpentContextV1 &other_context)
{
    // 1. spent status (higher statuses are assumed to be 'older')
    if (context.spent_status > other_context.spent_status)
        return true;
    if (context.spent_status < other_context.spent_status)
        return false;

    // 2. block index
    if (context.block_index < other_context.block_index)
        return true;
    if (context.block_index > other_context.block_index)
        return false;

    // 3. block timestamp
    if (context.block_timestamp < other_context.block_timestamp)
        return true;
    if (context.block_timestamp > other_context.block_timestamp)
        return false;

    return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool have_same_destination(const LegacyContextualBasicEnoteRecordV1 &a, const LegacyContextualBasicEnoteRecordV1 &b)
{
    return onetime_address_ref(a.record.enote) == onetime_address_ref(b.record.enote);
}
//-------------------------------------------------------------------------------------------------------------------
bool have_same_destination(const LegacyContextualIntermediateEnoteRecordVariant &a,
    const LegacyContextualIntermediateEnoteRecordVariant &b)
{
    return onetime_address_ref(enote_record_ref(a).enote) == onetime_address_ref(enote_record_ref(b).enote);
}
//-------------------------------------------------------------------------------------------------------------------
bool have_same_destination(const LegacyContextualEnoteRecordVariant &a, const LegacyContextualEnoteRecordVariant &b)
{
    return onetime_address_ref(enote_record_ref(a).enote) == onetime_address_ref(enote_record_ref(b).enote);
}
//-------------------------------------------------------------------------------------------------------------------
bool have_same_destination(const SpContextualBasicEnoteRecordV1 &a, const SpContextualBasicEnoteRecordV1 &b)
{
    return onetime_address_ref(a.record.enote) == onetime_address_ref(b.record.enote);
}
//-------------------------------------------------------------------------------------------------------------------
bool have_same_destination(const SpContextualIntermediateEnoteRecordV1 &a,
        const SpContextualIntermediateEnoteRecordV1 &b)
{
    return onetime_address_ref(a) == onetime_address_ref(b);
}
//-------------------------------------------------------------------------------------------------------------------
bool have_same_destination(const SpContextualEnoteRecordV1 &a, const SpContextualEnoteRecordV1 &b)
{
    return onetime_address_ref(a.record.enote) == onetime_address_ref(b.record.enote);
}
//-------------------------------------------------------------------------------------------------------------------
bool has_origin_status(const LegacyContextualIntermediateEnoteRecordVariant &record,
        const SpEnoteOriginStatus test_status)
{
    return origin_status_ref(record) == test_status;
}
//-------------------------------------------------------------------------------------------------------------------
bool has_origin_status(const LegacyContextualEnoteRecordVariant &record, const SpEnoteOriginStatus test_status)
{
    return origin_status_ref(record) == test_status;
}
//-------------------------------------------------------------------------------------------------------------------
bool has_origin_status(const SpContextualIntermediateEnoteRecordV1 &record, const SpEnoteOriginStatus test_status)
{
    return record.origin_context.origin_status == test_status;
}
//-------------------------------------------------------------------------------------------------------------------
bool has_origin_status(const SpContextualEnoteRecordV1 &record, const SpEnoteOriginStatus test_status)
{
    return record.origin_context.origin_status == test_status;
}
//-------------------------------------------------------------------------------------------------------------------
bool has_spent_status(const LegacyContextualEnoteRecordVariant &record, const SpEnoteSpentStatus test_status)
{
    return spent_context_ref(record).spent_status == test_status;
}
//-------------------------------------------------------------------------------------------------------------------
bool has_spent_status(const SpContextualEnoteRecordV1 &record, const SpEnoteSpentStatus test_status)
{
    return record.spent_context.spent_status == test_status;
}
//-------------------------------------------------------------------------------------------------------------------
bool has_key_image(const SpContextualKeyImageSetV1 &key_image_set, const crypto::key_image &test_key_image)
{
    return std::find(key_image_set.legacy_key_images.begin(), key_image_set.legacy_key_images.end(), test_key_image) !=
            key_image_set.legacy_key_images.end() ||
        std::find(key_image_set.sp_key_images.begin(), key_image_set.sp_key_images.end(), test_key_image) !=
            key_image_set.sp_key_images.end();
}
//-------------------------------------------------------------------------------------------------------------------
} //namespace sp
