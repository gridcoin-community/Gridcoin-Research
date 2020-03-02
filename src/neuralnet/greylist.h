#pragma once

#include "neuralnet/project.h"

namespace NN {
//!
//! \brief Describes why a BOINC project is temporarily greylisted.
//!
enum class GreylistReason
{
    NONE,              //!< Project is not greylisted.
    WORK_AVAILABILITY, //!< Failed to supply a reasonable quantity of work.
    ZERO_CREDIT_DAYS,  //!< Exported too many recent zero-credit days.
};

//!
//! \brief Associates a project on the Gridcoin whitelist with a reason for
//! why the protocol automatically placed it on the greylist.
//!
class GreylistProject
{
public:
    const Project* const m_project; //!< Refers to the greylisted project.
    const GreylistReason m_reason;  //!< Describes why a project is greylisted.

    //!
    //! \brief Initialize a new greylisted project context.
    //!
    //! \param project Refers to the greylisted project.
    //! \param reason  Describes why a project is greylisted.
    //!
    GreylistProject(const Project* const project, const GreylistReason reason)
        : m_project(project)
        , m_reason(reason)
    {
    }

    const Project& operator*() const
    {
        return *m_project;
    }

    const Project* operator->() const
    {
        return m_project;
    }
};

//!
//! \brief Contains a snapshot of projects on the Gridcoin whitelist grouped
//! by greylisted status.
//!
class GreylistSnapshot
{
public:
    typedef std::vector<const Project*> ActiveProjectSet;
    typedef std::vector<GreylistProject> GreylistedProjectSet;

    //!
    //! \brief Initialize a new greylist snapshot instance.
    //!
    //! \param projects            A snapshot of the projects on the whitelist.
    //! \param active_projects     Groups non-greylisted projects.
    //! \param greylisted_projects Groups greylisted projects.
    //!
    GreylistSnapshot(
        WhitelistSnapshot projects,
        ActiveProjectSet active_projects,
        GreylistedProjectSet greylisted_projects)
        : m_projects(std::move(projects))
        , m_active_projects(std::move(active_projects))
        , m_greylisted_projects(std::move(greylisted_projects))
    {
    }

    //!
    //! \brief Get the set of non-greylisted projects on the whitelist.
    //!
    //! \return Projects that did not trigger an automated greylisting rule.
    //!
    const ActiveProjectSet& ActiveProjects() const
    {
        return m_active_projects;
    }

    //!
    //! \brief Get the set of greylisted projects on the whitelist.
    //!
    //! \return Projects that triggered automated greylisting rules, if any.
    //!
    const GreylistedProjectSet& GreylistedProjects() const
    {
        return m_greylisted_projects;
    }

    //!
    //! \brief Determine whether the snapshot contains an active or greylisted
    //! project with the specified name.
    //!
    //! \param project_name Identifies the project to check for.
    //!
    //! \return \c true if either the active set or greylisted set of projects
    //! contains the project identified by \p project_name.
    //!
    bool Contains(const std::string& project_name) const
    {
        return m_projects.Contains(project_name);
    }

    //!
    //! \brief Determine whether the specified project triggered an automated
    //! greylisting rule.
    //!
    //! \param project_name Identifies the project to check for.
    //!
    //! \return A value that describes the greylist status of the project.
    //!
    GreylistReason Check(const std::string& project_name) const
    {
        for (const auto& project : m_greylisted_projects) {
            if (project->m_name == project_name) {
                return project.m_reason;
            }
        }

        return GreylistReason::NONE;
    }

    //!
    //! \brief Determine whether the specified project is not greylisted.
    //!
    //! \param project_name Identifies the project to check for.
    //!
    //! \return \c true if the project did not trigger an automated greylisting
    //! rule.
    //!
    bool CheckActive(const std::string& project_name) const
    {
        return Check(project_name) == GreylistReason::NONE;
    }

    //!
    //! \brief Determine whether the specified project is greylisted.
    //!
    //! \param project_name Identifies the project to check for.
    //!
    //! \return \c true if the project triggered an automated greylisting rule.
    //!
    bool CheckGreylisted(const std::string& project_name) const
    {
        return Check(project_name) != GreylistReason::NONE;
    }

private:
    //!
    //! \brief Contains a snapshot of the full set of whitelisted projects.
    //!
    //! This snapshot contains both active and greylisted projects. By storing
    //! the snapshot here, \c GreylistSnapshot objects inherit the thread-safe
    //! design of \c WhitelistSnapshot. The active and greylisted groups point
    //! to project objects contained in this field.
    //!
    const WhitelistSnapshot m_projects;

    //!
    //! \brief Projects not associated with a greylist status.
    //!
    //! Pointers in this set refer to objects in \c m_projects. Dereferencing
    //! these is always safe.
    //!
    const ActiveProjectSet m_active_projects;

    //!
    //! \brief Projects associated with a greylist status.
    //!
    //! Pointers in this set refer to objects in \c m_projects. Dereferencing
    //! these is always safe.
    //!
    const GreylistedProjectSet m_greylisted_projects;
};

//!
//! \brief Manages automated greylisting of projects on the Gridcoin whitelist.
//!
//! The network may temporarily disqualify whitelisted BOINC projects from the
//! reward protocol because of conditions that render a project unsuitable for
//! participation. For these cases, a project is placed onto the "greylist" to
//! classify the project as ineligible for reward until it resolves short-term
//! operational issues. This class manages the state used to identify projects
//! in a greylisted status for the rules compatible with automation.
//!
class Greylist
{
public:
    //!
    //! \brief Filter the supplied set of whitelisted projects into active
    //! and greylisted statuses.
    //!
    //! \param projects A snapshot of projects on the whitelist to filter.
    //!
    //! \return A snapshot of projects categorized by greylist status.
    //!
    GreylistSnapshot Filter(WhitelistSnapshot projects);

    //!
    //! \brief Determine whether the supplied project triggered an automated
    //! greylisting rule.
    //!
    //! \param project The whitelisted project to check for.
    //!
    //! \return A value that describes the greylist status of the project.
    //!
    GreylistReason Check(const Project& project);

    //!
    //! \brief Determine whether the specified project triggered an automated
    //! greylisting rule.
    //!
    //! \param project_name The name of the whitelisted project to check for.
    //!
    //! \return A value that describes the greylist status of the project.
    //!
    GreylistReason Check(const std::string& project_name);
};
}
