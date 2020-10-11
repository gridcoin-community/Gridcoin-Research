// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"

#include <algorithm>
#include <atomic>

using namespace GRC;

namespace
{
    Whitelist whitelist;
}

Whitelist& GRC::GetWhitelist()
{
    return whitelist;
}

// -----------------------------------------------------------------------------
// Class: Project
// -----------------------------------------------------------------------------

constexpr uint32_t Project::CURRENT_VERSION; // For clang

Project::Project() : m_timestamp(0)
{
}

Project::Project(std::string name, std::string url)
    : Project(std::move(name), std::move(url), 0)
{
}

Project::Project(std::string name, std::string url, int64_t timestamp)
    : m_name(std::move(name)), m_url(std::move(url)), m_timestamp(timestamp)
{
}

std::string Project::DisplayName() const
{
    std::string display_name = m_name;
    std::replace(display_name.begin(), display_name.end(), '_', ' ');

    return display_name;
}

std::string Project::BaseUrl() const
{
    // Remove the "@" from the URL in the contract. We assume that it always
    // occurs at the very end:
    return m_url.substr(0, m_url.size() - 1);
}

std::string Project::DisplayUrl() const
{
    // TODO: remove this after project contracts support arbitrary URLs.
    // WCG project URL refers to a location inaccessible to the end user.
    if (m_name == "World_Community_Grid") {
        return "https://www.worldcommunitygrid.org/";
    }

    return BaseUrl();
}

std::string Project::StatsUrl(const std::string& type) const
{
    if (type.empty()) {
        return BaseUrl() + "stats/";
    }

    return BaseUrl() + "stats/" + type + ".gz";
}

// -----------------------------------------------------------------------------
// Class: WhitelistSnapshot
// -----------------------------------------------------------------------------

WhitelistSnapshot::WhitelistSnapshot(ProjectListPtr projects)
    : m_projects(std::move(projects))
{
}

WhitelistSnapshot::const_iterator WhitelistSnapshot::begin() const
{
    return m_projects->begin();
}

WhitelistSnapshot::const_iterator WhitelistSnapshot::end() const
{
    return m_projects->end();
}

WhitelistSnapshot::size_type WhitelistSnapshot::size() const
{
    return m_projects->size();
}

bool WhitelistSnapshot::Populated() const
{
    return !m_projects->empty();
}

bool WhitelistSnapshot::Contains(const std::string& name) const
{
    if (name.empty()) {
        return false;
    }

    // We could store an unordered map to speed this up, but with so few items
    // in the whitelist, the overhead of a map isn't worth it. Iteration is our
    // primary use-case, and iteration over the vector is fast.
    for (const auto& project : *m_projects) {
        if (project.m_name == name) {
            return true;
        }
    }

    return false;
}

WhitelistSnapshot WhitelistSnapshot::Sorted() const
{
    ProjectList sorted(m_projects->begin(), m_projects->end());

    auto ascending_by_name = [](const Project& a, const Project& b) {
        return std::lexicographical_compare(
            a.m_name.begin(),
            a.m_name.end(),
            b.m_name.begin(),
            b.m_name.end(),
            [](const char ac, const char bc) {
                return std::tolower(ac) < std::tolower(bc);
            });
    };

    std::sort(sorted.begin(), sorted.end(), ascending_by_name);

    return WhitelistSnapshot(std::make_shared<ProjectList>(sorted));
}

// -----------------------------------------------------------------------------
// Class: Whitelist
// -----------------------------------------------------------------------------

Whitelist::Whitelist()
    : m_projects(std::make_shared<ProjectList>())
{
}

WhitelistSnapshot Whitelist::Snapshot() const
{
    // With C++20, use std::atomic<std::shared_ptr<T>>::load() instead:
    return WhitelistSnapshot(std::atomic_load(&m_projects));
}

void Whitelist::Reset()
{
    std::atomic_store(&m_projects, std::make_shared<ProjectList>());
}

void Whitelist::Add(const ContractContext& ctx)
{
    Project project = ctx->CopyPayloadAs<Project>();
    project.m_timestamp = ctx.m_tx.nTime;

    ProjectListPtr copy = CopyFilteredWhitelist(project.m_name);

    copy->emplace_back(std::move(project));

    // With C++20, use std::atomic<std::shared_ptr<T>>::store() instead:
    std::atomic_store(&m_projects, std::move(copy));
}

void Whitelist::Delete(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<Project>();

    // With C++20, use std::atomic<std::shared_ptr<T>>::store() instead:
    std::atomic_store(&m_projects, CopyFilteredWhitelist(payload->m_name));
}

ProjectListPtr Whitelist::CopyFilteredWhitelist(const std::string& name) const
{
    ProjectListPtr copy = std::make_shared<ProjectList>();

    for (const auto& project : *m_projects) {
        if (project.m_name != name) {
            copy->push_back(project);
        }
    }

    return copy;
}
