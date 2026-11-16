/*
    A modal prompt of the wallet operations: a title, a text, and either Yes
    with a negative button or OK alone. Every close answers once, from
    onClosed: Yes answers yes, and every other close (the negative button,
    Escape, Return where the default is no, a closed window) answers no.
*/
import QtQuick
import QtQuick.Controls
import MMPTheme 1.0

Dialog {
    id: promptDialog

    // The text under the title, and its format: Text.PlainText, or
    // Text.RichText for a text built from escaped values.
    property string text
    property int textFormat: Text.PlainText
    // True: Yes and the negative button. False: OK alone.
    property bool question: true
    // The negative button's label.
    property string noText: mnemonicFree(qsTranslate("OptionsDialog", "&Cancel"))
    // The answer Return gives.
    property bool defaultYes: false
    // Set by Yes; read when the prompt closes.
    property bool yesChosen: false

    // Emitted once per open, when the prompt has closed.
    signal answered(bool yes)

    // The OK and Cancel labels reuse OptionsDialog's "&OK" and "&Cancel",
    // whose translations already exist. Their mnemonic marker is removed, so
    // no style shows it: a "(&X)" group and the space before it first, then
    // "&&" to "&" and "&X" to "X".
    function mnemonicFree(s) {
        return s.replace(/\s*\(&([^&])\)/g, "").replace(/&(&?)/g, "$1")
    }

    function answer(yes) {
        promptDialog.yesChosen = yes
        promptDialog.close()
    }

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: 440
    anchors.centerIn: parent

    onAboutToShow: promptDialog.yesChosen = false
    onClosed: promptDialog.answered(promptDialog.yesChosen)

    background: Rectangle {
        anchors.fill: parent
        color: MMPTheme.headerColor
        radius: 4
        border {
            color: MMPTheme.borderColor
            width: 1
        }
    }

    header: Label {
        text: promptDialog.title
        textFormat: Text.PlainText
        font.bold: true
        padding: 15
        color: MMPTheme.textColor
        background: Rectangle {
            color: MMPTheme.ternaryBodyColor
            anchors.fill: parent
            radius: 4
            anchors.margins: 1
        }
    }

    contentItem: Item {
        implicitHeight: promptText.implicitHeight + 20
        focus: true
        Keys.onReturnPressed: promptDialog.answer(!promptDialog.question || promptDialog.defaultYes)
        Keys.onEnterPressed: promptDialog.answer(!promptDialog.question || promptDialog.defaultYes)

        Text {
            id: promptText
            objectName: "promptText"
            text: promptDialog.text
            textFormat: promptDialog.textFormat
            wrapMode: Text.Wrap
            color: MMPTheme.textColor
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 10
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            objectName: "promptYes"
            visible: promptDialog.question
            text: qsTr("Yes")
            onClicked: promptDialog.answer(true)
        }
        Button {
            objectName: "promptNo"
            visible: promptDialog.question
            text: promptDialog.noText
            onClicked: promptDialog.answer(false)
        }
        Button {
            objectName: "promptOk"
            visible: !promptDialog.question
            text: promptDialog.mnemonicFree(qsTranslate("OptionsDialog", "&OK"))
            onClicked: promptDialog.answer(false)
        }

        background: Rectangle {
            anchors.fill: parent
            color: MMPTheme.ternaryBodyColor
            radius: 4
            anchors.margins: 1
        }
    }
}
