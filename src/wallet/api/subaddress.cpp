// Copyright (c) 2017-2023, The Monero Project
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

#include "subaddress.h"
#include "wallet.h"
#include "crypto/hash.h"
#include "wallet/wallet2.h"
#include "common_defines.h"

#include <vector>

namespace Monero {
  
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
static uint32_t get_subaddress_clamped_sum(uint32_t idx, uint32_t extra)
{
    static constexpr uint32_t uint32_max = std::numeric_limits<uint32_t>::max();
    if (idx > uint32_max - extra)
        return uint32_max;
    return idx + extra;
}
//-------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------
Subaddress::~Subaddress() {}
  
SubaddressImpl::SubaddressImpl(WalletImpl *wallet)
    : m_wallet(wallet) {}

void SubaddressImpl::addRow(uint32_t accountIndex, const std::string &label)
{
  m_wallet->m_wallet->add_subaddress(accountIndex, label);
  refresh(accountIndex);
}

void SubaddressImpl::setLabel(uint32_t accountIndex, uint32_t addressIndex, const std::string &label)
{
  try
  {
    m_wallet->m_wallet->set_subaddress_label({accountIndex, addressIndex}, label);
    refresh(accountIndex);
  }
  catch (const std::exception& e)
  {
    LOG_ERROR("setLabel: " << e.what());
  }
}

void SubaddressImpl::refresh(uint32_t accountIndex) 
{
  LOG_PRINT_L2("Refreshing subaddress");
  
  clearRows();
  for (size_t i = 0; i < m_wallet->m_wallet->get_num_subaddresses(accountIndex); ++i)
  {
    m_rows.push_back(new SubaddressRow(i, m_wallet->m_wallet->get_subaddress_as_str({accountIndex, (uint32_t)i}), m_wallet->m_wallet->get_subaddress_label({accountIndex, (uint32_t)i})));
  }
}

void SubaddressImpl::clearRows() {
   for (auto r : m_rows) {
     delete r;
   }
   m_rows.clear();
}

std::vector<SubaddressRow*> SubaddressImpl::getAll() const
{
  return m_rows;
}

SubaddressImpl::~SubaddressImpl()
{
  clearRows();
}

//-------------------------------------------------------------------------------------------------------------------
void SubaddressImpl::add_subaddress_account(const std::string &label)
{
    uint32_t index_major = (uint32_t)get_num_subaddress_accounts();
    expand_subaddresses({index_major, 0});
    m_wallet->m_wallet_settings->m_subaddress_labels[index_major][0] = label;
}
//-------------------------------------------------------------------------------------------------------------------
void SubaddressImpl::expand_subaddresses(const cryptonote::subaddress_index &index)
{
    WalletSettings *wallet_settings = m_wallet->m_wallet_settings.get();
    hw::device &hwdev = m_wallet->m_account.get_device();
    if (wallet_settings->m_subaddress_labels.size() <= index.major)
    {
        // add new accounts
        cryptonote::subaddress_index index2;
        const uint32_t major_end = get_subaddress_clamped_sum(index.major, wallet_settings->m_subaddress_lookahead_major);
        for (index2.major = wallet_settings->m_subaddress_labels.size(); index2.major < major_end; ++index2.major)
        {
            const uint32_t end = get_subaddress_clamped_sum((index2.major == index.major ? index.minor : 0), wallet_settings->m_subaddress_lookahead_minor);
            const std::vector<crypto::public_key> pkeys = hwdev.get_subaddress_spend_public_keys(m_wallet->m_account.get_keys(), index2.major, 0, end);
            for (index2.minor = 0; index2.minor < end; ++index2.minor)
            {
                const crypto::public_key &D = pkeys[index2.minor];
                wallet_settings->m_subaddresses[D] = index2;
            }
        }
        wallet_settings->m_subaddress_labels.resize(index.major + 1, {"Untitled account"});
        wallet_settings->m_subaddress_labels[index.major].resize(index.minor + 1);
        // TODO NOW
//        get_account_tags();
    }
    else if (wallet_settings->m_subaddress_labels[index.major].size() <= index.minor)
    {
        // add new subaddresses
        const uint32_t end = get_subaddress_clamped_sum(index.minor, wallet_settings->m_subaddress_lookahead_minor);
        const uint32_t begin = wallet_settings->m_subaddress_labels[index.major].size();
        cryptonote::subaddress_index index2 = {index.major, begin};
        const std::vector<crypto::public_key> pkeys = hwdev.get_subaddress_spend_public_keys(m_wallet->m_account.get_keys(), index2.major, index2.minor, end);
        for (; index2.minor < end; ++index2.minor)
        {
            const crypto::public_key &D = pkeys[index2.minor - begin];
            wallet_settings->m_subaddresses[D] = index2;
        }
        wallet_settings->m_subaddress_labels[index.major].resize(index.minor + 1);
    }
}
//-------------------------------------------------------------------------------------------------------------------
size_t SubaddressImpl::get_num_subaddress_accounts() const { return m_wallet->m_wallet_settings->m_subaddress_labels.size(); }
//-------------------------------------------------------------------------------------------------------------------
size_t SubaddressImpl::get_num_subaddresses(uint32_t index_major) const
{
    return index_major < m_wallet->m_wallet_settings->m_subaddress_labels.size() ? m_wallet->m_wallet_settings->m_subaddress_labels[index_major].size() : 0;
}
//-------------------------------------------------------------------------------------------------------------------

} // namespace
