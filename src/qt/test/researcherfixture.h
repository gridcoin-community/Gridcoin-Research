// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_RESEARCHERFIXTURE_H
#define BITCOIN_QT_TEST_RESEARCHERFIXTURE_H

#include "qt/guistatecache.h"
#include "qt/researcher/researchermodel.h"
#include "qt/test/interfacefakes.h"

#include <QCoreApplication>

#include <cstdint>

namespace qt_test {

//! A ResearcherModel on the fakes it stands on. The model reads the researcher
//! part of the pushed state, so a case sets that part with Push (or in
//! m_gui_state.m_snapshot before the fixture is built, for a snapshot already
//! computed at startup) and never through the researcher context.
struct ResearcherFixture
{
    FakeGuiState m_gui_state;
    FakeResearcherContext m_context;
    GuiStateCache m_cache;
    ResearcherModel m_model;

    ResearcherFixture()
        : m_cache(m_gui_state)
        , m_model(m_context, m_gui_state, m_cache)
    {
    }

    //! A state with the researcher part \p researcher, as the node pushes one
    //! while synced.
    static interfaces::GuiStateSnapshot Pushed(uint64_t sequence, const interfaces::ResearcherSnapshot& researcher)
    {
        interfaces::GuiStateSnapshot snapshot;
        snapshot.sequence = sequence;
        snapshot.out_of_sync = false;
        snapshot.has_researcher = true;
        snapshot.researcher = researcher;
        return snapshot;
    }

    //! A state without the researcher part, as the node pushes while out of sync.
    static interfaces::GuiStateSnapshot PushedAbsent(uint64_t sequence)
    {
        interfaces::GuiStateSnapshot snapshot;
        snapshot.sequence = sequence;
        return snapshot;
    }

    //! Push \p snapshot and deliver the queued hop to the GUI thread.
    void Push(const interfaces::GuiStateSnapshot& snapshot)
    {
        m_gui_state.Push(snapshot);
        QCoreApplication::sendPostedEvents(nullptr, 0);
    }
};

} // namespace qt_test

#endif // BITCOIN_QT_TEST_RESEARCHERFIXTURE_H
