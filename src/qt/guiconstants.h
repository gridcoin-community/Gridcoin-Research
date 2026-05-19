#ifndef BITCOIN_QT_GUICONSTANTS_H
#define BITCOIN_QT_GUICONSTANTS_H

#include <cstdint>

/* Milliseconds between model updates */
static const int MODEL_UPDATE_DELAY = 4000;

/* Milliseconds between WalletEventQueue drains. The producer→GUI event channel
 * for transaction-list updates is drained on this cadence; 500ms is
 * imperceptible to users while keeping each drain batch sized for efficient
 * Qt model mutation. */
static const int MODEL_EVENT_DRAIN_INTERVAL = 500;

/* Maximum WalletEventQueue events applied to the model in a single drain tick.
 * Bounds the Qt main-thread work per tick so a large backlog (reorg flood, IBD
 * catch-up) cannot freeze the GUI in one apply pass. When a drain hits this
 * cap the consumer re-arms itself immediately (0ms) rather than waiting for
 * the next periodic tick, so a backlog still drains promptly while yielding to
 * the Qt event loop between batches. */
static const int MODEL_EVENT_DRAIN_MAX_BATCH = 1024;

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* Invalid field background style */
#define STYLE_INVALID "background:#FF8080"

/* Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(128, 128, 128)
/* Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(255, 0, 0)
/* Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(140, 140, 140)

/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Maximum allowed URI length */
static const int MAX_URI_LENGTH = 255;

/* QRCodeDialog -- size of exported QR Code image */
#define EXPORT_IMAGE_SIZE 256

/* One gigabyte (GB) in bytes */
static constexpr uint64_t GB_BYTES{1000000000};

#endif // BITCOIN_QT_GUICONSTANTS_H
