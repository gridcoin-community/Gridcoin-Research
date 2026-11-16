/*
    The wallet operations' prompts: the send confirmation, the fee question,
    the send result and the out-of-sync warning. They live in the main
    window, so a prompt never closes with the view that asked for it.
*/
import QtQuick
import Gridcoin.Qml

Item {
    id: walletPrompts
    anchors.fill: parent

    // The request the open send confirmation and fee question answer; 0 while
    // none is open.
    property double sendConfirmId: 0
    property double feeId: 0
    // Results that arrived while resultPrompt was open or waiting to reopen,
    // as {title, text}, in arrival order.
    property var pendingResults: []
    // The out-of-sync question's two answers.
    property var outOfSyncYes: null
    property var outOfSyncNo: null

    // Shows a result, or queues it behind the one on show: a send result and
    // the rejection of a URI that waited for it can arrive together, and
    // neither is written over the other.
    function showResult(title, text) {
        if (resultPrompt.visible || walletPrompts.pendingResults.length > 0) {
            walletPrompts.pendingResults.push({ "title": title, "text": text })
            return
        }
        resultPrompt.title = title
        resultPrompt.text = text
        resultPrompt.open()
    }

    function showNextResult() {
        if (walletPrompts.pendingResults.length === 0) return
        var next = walletPrompts.pendingResults.shift()
        resultPrompt.title = next.title
        resultPrompt.text = next.text
        resultPrompt.open()
    }

    // Asks whether to open the send page while the wallet is out of sync;
    // calls onYes or onNo with the answer.
    function askOutOfSync(onYes, onNo) {
        walletPrompts.outOfSyncYes = onYes
        walletPrompts.outOfSyncNo = onNo
        outOfSyncPrompt.title = SendCoins.outOfSyncTitle()
        outOfSyncPrompt.text = SendCoins.outOfSyncText()
        outOfSyncPrompt.open()
    }

    Connections {
        target: SendCoins
        function onSendConfirmationRequested(id, title, text) {
            walletPrompts.sendConfirmId = id
            sendConfirmPrompt.title = title
            sendConfirmPrompt.text = text
            sendConfirmPrompt.open()
        }
        function onFeeConfirmationRequested(id, fee, title, text) {
            walletPrompts.feeId = id
            feePrompt.title = title
            feePrompt.text = text
            feePrompt.open()
        }
        // A sent or aborted request carries no message and shows nothing.
        function onFinished(id, outcome, title, message, txid) {
            if (message !== "") walletPrompts.showResult(title, message)
        }
        function onUriRejected(title, text) {
            walletPrompts.showResult(title, text)
        }
    }

    WalletPromptDialog {
        id: sendConfirmPrompt
        objectName: "sendConfirmPrompt"
        textFormat: Text.RichText
        onAnswered: (yes) => {
            var id = walletPrompts.sendConfirmId
            walletPrompts.sendConfirmId = 0
            if (id !== 0) SendCoins.confirmSend(id, yes)
        }
    }

    WalletPromptDialog {
        id: feePrompt
        objectName: "feePrompt"
        defaultYes: true
        onAnswered: (yes) => {
            var id = walletPrompts.feeId
            walletPrompts.feeId = 0
            if (id !== 0) SendCoins.confirmFee(id, yes)
        }
    }

    WalletPromptDialog {
        id: resultPrompt
        objectName: "resultPrompt"
        question: false
        onAnswered: (yes) => Qt.callLater(walletPrompts.showNextResult)
    }

    WalletPromptDialog {
        id: outOfSyncPrompt
        objectName: "outOfSyncPrompt"
        noText: qsTr("No")
        onAnswered: (yes) => {
            var onYes = walletPrompts.outOfSyncYes
            var onNo = walletPrompts.outOfSyncNo
            walletPrompts.outOfSyncYes = null
            walletPrompts.outOfSyncNo = null
            if (yes) {
                if (onYes) onYes()
            } else {
                if (onNo) onNo()
            }
        }
    }
}
