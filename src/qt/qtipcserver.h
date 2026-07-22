#ifndef BITCOIN_QT_QTIPCSERVER_H
#define BITCOIN_QT_QTIPCSERVER_H

// Define Gridcoin-Qt message queue name
#define BITCOINURI_QUEUE_NAME "GridcoinURI"

void ipcScanRelay(int argc, char *argv[]);
void ipcInit(int argc, char *argv[]);

//! Signal the URI-listener thread started by ipcInit() to exit. Called from the
//! GUI composition root once the Qt event loop has returned. This is a
//! GUI-process-local stop flag: the URI server belongs to this GUI process, so
//! its lifetime is bounded by the GUI quitting, not by core shutdown state (a
//! separate process in the multiprocess build).
void ipcShutdown();

#endif // BITCOIN_QT_QTIPCSERVER_H
