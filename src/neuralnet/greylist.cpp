#include "neuralnet/greylist.h"

using namespace NN;

GreylistSnapshot Greylist::Filter(WhitelistSnapshot projects)
{
    GreylistSnapshot::ActiveProjectSet active_projects;
    GreylistSnapshot::GreylistedProjectSet greylisted_projects;

    for (const auto& project : projects) {
        const GreylistReason reason = Check(project);

        if (reason == GreylistReason::NONE) {
            active_projects.emplace_back(&project);
        } else {
            greylisted_projects.emplace_back(&project, reason);
        }
    }

    return GreylistSnapshot(
        std::move(projects),
        std::move(active_projects),
        std::move(greylisted_projects));
}

GreylistReason Greylist::Check(const Project& project)
{
    return Check(project.m_name);
}

GreylistReason Greylist::Check(const std::string& project_name)
{
    // TODO: implement greylist rules
    return GreylistReason::NONE;
}
