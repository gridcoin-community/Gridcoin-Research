#ifndef BITCOIN_QT_QTIPCSERVER_H
#define BITCOIN_QT_QTIPCSERVER_H

#include <functional>
#include <string>

// Define Gridcoin-Qt message queue name
#define BITCOINURI_QUEUE_NAME "GridcoinURI"

void ipcScanRelay(int argc, char *argv[]);

//! Start the URI-listener thread. \p uri_handler is invoked (on the listener
//! thread) with each "gridcoin:" URI received from another instance; the GUI
//! composition root supplies a handler that marshals the URI onto the GUI
//! thread. Passing the handler in directly replaces the former core
//! uiInterface.ThreadSafeHandleURI signal round-trip -- URI dispatch is entirely
//! within this GUI process, so it needs no core boundary.
void ipcInit(int argc, char *argv[], std::function<void(const std::string&)> uri_handler);

//! Signal the URI-listener thread started by ipcInit() to exit. Called from the
//! GUI composition root once the Qt event loop has returned. This is a
//! GUI-process-local stop flag: the URI server belongs to this GUI process, so
//! its lifetime is bounded by the GUI quitting, not by core shutdown state (a
//! separate process in the multiprocess build).
void ipcShutdown();

#endif // BITCOIN_QT_QTIPCSERVER_H
