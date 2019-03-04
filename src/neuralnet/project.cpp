#include "project.h"

#include <algorithm>
#include <atomic>

using namespace NN;

// -----------------------------------------------------------------------------
// Class: Project
// -----------------------------------------------------------------------------

Project::Project(const std::string name, const std::string url, const int64_t ts)
    : m_name(name), m_url(url), m_timestamp(ts)
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

WhitelistSnapshot::WhitelistSnapshot(const ProjectListPtr projects)
    : m_projects(projects)
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

// -----------------------------------------------------------------------------
// Class: Whitelist
// -----------------------------------------------------------------------------

Whitelist::Whitelist()
    : m_projects(std::make_shared<std::vector<Project>>())
{
}

WhitelistSnapshot Whitelist::Snapshot() const
{
    // With C++20, use std::atomic<std::shared_ptr<T>>::load() instead:
    return WhitelistSnapshot(std::atomic_load(&m_projects));
}

void Whitelist::Add(
    const std::string& name,
    const std::string& url,
    const int64_t& timestamp)
{
    ProjectListPtr copy = std::make_shared<std::vector<Project>>(*m_projects);

    copy->emplace_back(name, url, timestamp);

    // With C++20, use std::atomic<std::shared_ptr<T>>::store() instead:
    std::atomic_store(&m_projects, copy);
}

void Whitelist::Delete(const std::string& name)
{
    ProjectListPtr copy = std::make_shared<std::vector<Project>>();

    for (const auto& project : *m_projects) {
        if (project.m_name != name) {
            copy->push_back(project);
        }
    }

    // With C++20, use std::atomic<std::shared_ptr<T>>::store() instead:
    std::atomic_store(&m_projects, copy);
}
