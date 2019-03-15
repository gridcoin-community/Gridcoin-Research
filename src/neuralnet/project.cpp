#include "project.h"

#include <algorithm>
#include <atomic>
#include <boost/algorithm/string/case_conv.hpp>

using namespace NN;

//!
//! \brief A mutable representation of a \c Project.
//!
//! The internal implementation uses this class to sort containers of \c Project
//! instances. \c std::sort() cannot sort a collection of \c const objects.
//!
struct MutableProject
{
    //!
    //! \brief Create a mutable representation of the provided \c Project.
    //!
    //! \param project The \c const instance to copy.
    //!
    MutableProject(const Project& project)
        : m_name(project.m_name),
          m_url(project.m_url),
          m_timestamp(project.m_timestamp)
    {
    }

    std::string m_name;   //!< As it exists in the contract key field.
    std::string m_url;    //!< As it exists in the contract value field.
    int64_t m_timestamp;  //!< Timestamp of the contract.

    //!
    //! \brief Get the lowercase equivalent of the project name.
    //!
    //! Lazy case-conversion. Used for sorting.
    //!
    //! \return The lowercase project name.
    //!
    std::string& LowercaseName() const
    {
        if (m_lower_name.empty()) {
            m_lower_name = m_name;
            boost::to_lower(m_lower_name);
        }

        return m_lower_name;
    }
namespace
{
    Whitelist whitelist;
}

private:
    mutable std::string m_lower_name; //!< Caches the lowercase project name.
};
Whitelist& NN::GetWhitelist()
{
    return whitelist;
}

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

WhitelistSnapshot WhitelistSnapshot::Sorted() const
{
    // TODO: we cannot sort a vector of const Project objects with std::sort()
    // because it uses move semantics, so we copy it (twice). Find a better way.
    std::vector<MutableProject> sorted;

    for (auto const& project : *m_projects) {
        sorted.emplace_back(project);
    }

    auto comparer = [] (const MutableProject& a, const MutableProject& b) {
        return a.LowercaseName() < b.LowercaseName();
    };

    std::sort(sorted.begin(), sorted.end(), comparer);

    ProjectListPtr copy = std::make_shared<std::vector<Project>>();

    for (auto const& project : sorted) {
        copy->emplace_back(project.m_name, project.m_url, project.m_timestamp);
    }

    return WhitelistSnapshot(copy);
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

void Whitelist::Add(
    const std::string& name,
    const std::string& url,
    const int64_t& timestamp)
{
    ProjectListPtr copy = std::make_shared<ProjectList>(*m_projects);

    copy->emplace_back(name, url, timestamp);

    // With C++20, use std::atomic<std::shared_ptr<T>>::store() instead:
    std::atomic_store(&m_projects, copy);
}

void Whitelist::Delete(const std::string& name)
{
    ProjectListPtr copy = std::make_shared<ProjectList>();

    for (const auto& project : *m_projects) {
        if (project.m_name != name) {
            copy->push_back(project);
        }
    }

    // With C++20, use std::atomic<std::shared_ptr<T>>::store() instead:
    std::atomic_store(&m_projects, copy);
}
