/*
    A window for the creation of polls
*/
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import MMPTheme 1.0

Window {
    id: window
    title: qsTr("Create Poll")
    width: 700
    height: 600
    minimumWidth: 480
    minimumHeight: 450
    flags: Qt.Dialog
    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: window.close()
    }

    StackView {
        id: stackView
        initialItem: pollChoiceView
        anchors.fill: parent
        onCurrentItemChanged: {
            if (currentItem.defaultFocusItem) currentItem.defaultFocusItem.forceActiveFocus()
        }
    }

    Component {
        id: pollChoiceView
        Rectangle {
            id: choiceBackground
            color: MMPTheme.backgroundColor
            property Item defaultFocusItem: pollTypeComboBox
            Rectangle {
                id: choiceBody
                color: MMPTheme.bodyColor
                radius: 4
                anchors {
                    top: parent.top
                    bottom: nextButton.top
                    left: parent.left
                    right: parent.right
                    margins: 10
                }
                ScrollView {
                    id: contentScrollView
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: createPollTitleLabel.implicitHeight + createPollTitleLabel.anchors.topMargin +
                                   communityGuidelinesLabel.implicitHeight + communityGuidelinesLabel.anchors.topMargin +
                                   guidelinesLink.implicitHeight + guidelinesLink.anchors.topMargin +
                                   separator.height + separator.anchors.topMargin +
                                   pollTypeLabel.implicitHeight + pollTypeLabel.anchors.topMargin +
                                   pollTypeDescription.implicitHeight + pollTypeDescription.anchors.topMargin +
                                   pollRequirementLabel.implicitHeight + pollRequirementLabel.anchors.topMargin

                    Text {
                        id: createPollTitleLabel
                        text: qsTr("Create a poll")
                        color: MMPTheme.textColor
                        font.weight: Font.DemiBold
                        font.pixelSize: 15
                        clip: true
                        anchors {
                            top: parent.top
                            left: parent.left
                            leftMargin: 30
                            topMargin: 20
                        }
                    }
                    Text {
                        id: communityGuidelinesLabel
                        text: qsTr("The Gridcoin community established guidelines for polls with requirements for each type. Please read the wiki for more information:")
                        color: MMPTheme.textColor
                        wrapMode: Text.WordWrap
                        anchors {
                            top: createPollTitleLabel.bottom
                            left: createPollTitleLabel.left
                            right: parent.right
                            topMargin: 20
                            rightMargin: 20

                        }
                    }
                    ClickableLink {
                        id: guidelinesLink
                        urlString: "https://gridcoin.us/wiki/voting.html"
                        displayString: "gridcoin.us/wiki/voting.html"
                        anchors {
                            top: communityGuidelinesLabel.bottom
                            left: communityGuidelinesLabel.left
                            topMargin: 10
                        }
                    }
                    Rectangle {
                        id: separator
                        height: 1
                        color: MMPTheme.separatorColor
                        anchors {
                            top: guidelinesLink.bottom
                            left: parent.left
                            right: parent.right
                            leftMargin: 30
                            rightMargin: 30
                            topMargin: 20
                        }
                    }
                    Text {
                        id: pollTypeLabel
                        text: qsTr("Poll Type:")
                        color: MMPTheme.textColor
                        anchors {
                            top: separator.bottom
                            left: separator.left
                            topMargin: 20
                        }
                    }
                    ComboBox {
                        id: pollTypeComboBox
                        focus: true
                        Component.onCompleted: forceActiveFocus()
                        KeyNavigation.tab: nextButton
                        Keys.onReturnPressed: nextButton.activate()
                        Keys.onEnterPressed: nextButton.activate()
                        model: [qsTr("Project Listing"), qsTr("Protocol Development"), qsTr("Governance"), "Marketing", "Outreach", "Community", "Survey"]
                        anchors {
                            left: pollTypeLabel.right
                            leftMargin: 10
                            verticalCenter: pollTypeLabel.verticalCenter
                        }
                    }
                    Text {
                        id: pollTypeDescription
                        color: MMPTheme.textColor
                        wrapMode: Text.WordWrap
                        anchors {
                            top: pollTypeLabel.bottom
                            left: parent.left
                            right: parent.right
                            leftMargin: 30
                            rightMargin: 30
                            topMargin: 20
                        }
                        text: {
                            switch (pollTypeComboBox.currentIndex) {
                            case 0:
                                return qsTr("Polls to remove or add projects to the whitelist. For an overview of the process that must be completed beforehand to add a project, please view the whitelist process wiki page. For the removal of a project, there must be an attempt at contact with the project admin before making the poll.")
                            case 1:
                                return qsTr("Development polls are for changing some parts of Gridcoin at a protocol level. For instance changing the amounts for rewards would be a development poll.")
                            case 2:
                                return qsTr("Mostly for changing the requirements of the polls. Defined in the currently voted in standard as “any proposal which seeks to modify the management or organizational structure of Gridcoin.”")
                            case 3:
                                return qsTr("Polls that propose marketing initiatives. For instance could be a plan to run an ad.")
                            case 4:
                                return qsTr("Polls about community outreach, representation, etc. Can be proactively trying to reach out to new places or keep up relations. Can also be retroactive to reimburse outreach as well.")
                            case 5:
                                return qsTr("Proposals or initiatives related to the Gridcoin community, but not to the other poll types.")
                            case 6:
                                return qsTr("These polls are non-binding so they don’t have any particular requirements. They can be specific or can focus on the broader picture. They can also be used to gauge community interest on something for development before more work is put in.")
                            default:
                                return ""
                            }
                        }
                    }
                    Text {
                        id: pollRequirementLabel
                        color: MMPTheme.textColor
                        wrapMode: Text.WordWrap
                        anchors {
                            top: pollTypeDescription.bottom
                            left: parent.left
                            right: parent.right
                            bottom: parent.bottom
                            leftMargin: 30
                            rightMargin: 30
                            topMargin: 10
                        }
                        text: {
                            switch (pollTypeComboBox.currentIndex) {
                            case 0:
                                return qsTr(
                                            "<p>Whitelist polls to add a project must have a project that meets the following criteria:</p>
        <ul>
        <li>Project Work Availability Score is green</li>
        <li>Number of Zero Credit Days is less than or equal to 7 out of the last 20 days</li>
        <li>The project has a clear description of the work and the work is as described</li>
        <li>The project allows new user registration</li>
        <li>Project complies with the BOINC terms of service</li>
        <li>All crunchers that comply with the terms of service of both BOINC and the project have equal access to work units</li>
        </ul>
        <p>Additionally, before a poll is made to whitelist there must be:</p>
        <ul>
        <li>Direct contact with a project administrator</li>
        <li>An affirmative public response to whitelisting by a project administrator</li>
        <li>An agreed add date with both the Gridcoin whitelist administrator and project admin</li>
        </ul>
        <p>Whitelist polls have a minimum duration of 3 weeks (21 days) and require an
        AVW of 40% or more</p>"
                                            )
                            case 1:
                                return qsTr("These types of polls require a minimum duration of 6 weeks (42 days) and development polls require an AVW of more than 50% to pass.")
                            case 2:
                                return qsTr("Governance polls require a minium duration of 3 weeks (21 days) and an AVW of 20% or more to pass")
                            case 3:
                                return qsTr("Marketing polls require a minimum duration of 3 weeks (21 days) and an AWV of 40% or more to pass")
                            case 4:
                                return qsTr("Outreach polls require a minimum duration of 3 weeks (21 days) and an AVW of 40% or more to pass")
                            case 5:
                                return qsTr("Requires a minimum duration of 3 weeks (21 days) and an AVW of 10% or more to pass")
                            case 6:
                            default:
                                return ""
                            }
                        }
                    }
                }
            }
            Button {
                id: nextButton
                text: qsTr("Next")
                icon.source: MMPTheme.themeSelect("/icons/generic/ic_chevron_right_light.svg", "/icons/generic/ic_chevron_right_dark.svg")
                onClicked: activate()
                Keys.onEnterPressed: activate()
                Keys.onReturnPressed: activate()
                KeyNavigation.tab: pollTypeComboBox
                function activate () {
                    var minPollLength
                    var weightTypeVal   //0: Balance, 1: Mag+Bal
                    var responseTypeVal //0: Y/N/Abs, 1: Single Choice, 2: Multiple Choice
                    switch (pollTypeComboBox.currentIndex){
                    case 0: //Project Listing
                        stackView.push(projectSelectionView)
                        return
                    case 1: //Protocol Development
                        minPollLength = 42
                        weightTypeVal = 1
                        break
                    case 2: //Governance
                    case 3: //Marketing
                    case 4: //Outreach
                    case 5: //Community
                        minPollLength = 21
                        weightTypeVal = 1
                        break;
                    case 6: //Survey
                    default:
                        break
                    }
                    stackView.push(pollCreationDetailView, {pollTypeString: pollTypeComboBox.currentText, minimumPollLength: minPollLength, weightType: weightTypeVal, responseType: responseTypeVal})
                }
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    margins: 10
                }
            }
        }
    }

    Component {
        id: pollCreationDetailView
        Rectangle {
            id: detailBackground
            color: MMPTheme.backgroundColor
            property Item defaultFocusItem: durationSpinbox
            property string pollTypeString: "Default"
            property string pollTitle
            property int minimumPollLength
            property int weightType
            property int responseType

            Rectangle {
                id: detailBody
                color: MMPTheme.bodyColor
                radius: 4
                clip: true
                anchors {
                    top: parent.top
                    bottom: createPollButton.top
                    left: parent.left
                    right: parent.right
                    margins: 10
                }
                ScrollView {
                    id: contentScrollView
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: createPollTitleLabel.implicitHeight + createPollTitleLabel.anchors.topMargin +
                                   separator.implicitHeight + separator.anchors.topMargin +
                                   formGrid.implicitHeight + formGrid.anchors.topMargin +
                                   20


                    Text {
                        id: createPollTitleLabel
                        text: qsTr("Poll Details")
                        color: MMPTheme.textColor
                        font.weight: Font.DemiBold
                        font.pixelSize: 15
                        clip: true
                        anchors {
                            top: parent.top
                            left: parent.left
                            leftMargin: 30
                            topMargin: 20
                        }
                    }
                    Rectangle {
                        id: separator
                        height: 1
                        color: MMPTheme.separatorColor
                        anchors {
                            top: createPollTitleLabel.bottom
                            left: parent.left
                            right: parent.right
                            leftMargin: 30
                            rightMargin: 30
                            topMargin: 20
                        }
                    }
                    GridLayout {
                        id: formGrid
                        rowSpacing: 20
                        columnSpacing: 20
                        columns: 2
                        anchors {
                            top: separator.bottom
                            left: parent.left
                            right: parent.right
                            leftMargin: 30
                            rightMargin: 30
                            topMargin: 20
                        }
                        Text {
                            id: lockedFieldsLabel
                            color: MMPTheme.highlightColor
                            visible: weightType || responseType
                            Layout.columnSpan: 2
                            text: qsTr("Some fields are locked for the selected poll type")
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            id: pollTypeLabel
                            text: qsTr("Poll Type:")
                            color: MMPTheme.textColor
                        }
                        Item {
                            id: pollTypeDurationItem
                            height: pollTypeBackgroundRect.height
                            Layout.fillWidth: true
                            Rectangle {
                                id: pollTypeBackgroundRect
                                color: MMPTheme.lightTextColor
                                height: pollTypeDataLabel.implicitHeight + 12
                                width: pollTypeDataLabel.implicitWidth + 20
                                radius: 4
                                anchors {
                                    left: parent.left
                                    verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    id: pollTypeDataLabel
                                    text: pollTypeString
                                    font.weight: Font.DemiBold
                                    color: MMPTheme.themeSelect(MMPTheme.cWhite, "#161b24")
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignHCenter
                                    anchors.fill: parent
                                }
                            }
                            SpinBox {
                                id: durationSpinbox
                                width: 60
                                value: minimumPollLength ? minimumPollLength : 21
                                from: minimumPollLength ? minimumPollLength : 1
                                to: 90
                                editable: true
                                anchors {
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                }
                            }
                            Text {
                                id: durationLabel
                                text: qsTr("Duration (days):")
                                color: MMPTheme.textColor
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignRight
                                anchors {
                                    right: durationSpinbox.left
                                    rightMargin: 10
                                    left: pollTypeBackgroundRect.right
                                    leftMargin: 10
                                    verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                        Text {
                            id: titleLabel
                            text: qsTr("Title: ")
                            color: MMPTheme.textColor
                        }
                        TextField {
                            id: titleTextEdit
                            text: pollTitle ? pollTitle : ""
                            Layout.fillWidth: true
                        }
                        Text {
                            id: questionLabel
                            text: qsTr("Question:")
                            color: MMPTheme.textColor
                        }
                        TextField {
                            id: questionTextField
                            Layout.fillWidth: true
                        }
                        Text {
                            id: discussionURLLabel
                            text: qsTr("Discussion URL:")
                            color: MMPTheme.textColor
                        }
                        TextField {
                            id: discussionURLTextField
                            Layout.fillWidth: true
                            KeyNavigation.tab: weightTypeComboBox
                        }
                        Text {
                            id: weightTypeLabel
                            text: qsTr("Weight Type:")
                            color: MMPTheme.textColor
                        }
                        ComboBox {
                            id: weightTypeComboBox
                            currentIndex: weightType ? weightType : 0
                            enabled: !weightType
                            model: [qsTr("Balance"), qsTr("Magnitude + Balance")]
                            KeyNavigation.tab: responseTypeComboBox
                        }
                        Text {
                            id: responseTypeLabel
                            text: qsTr("Response Type:")
                            color: MMPTheme.textColor
                        }
                        ComboBox {
                            id: responseTypeComboBox
                            currentIndex: responseType ? responseType : 0
                            enabled: !responseType
                            model: [qsTr("Yes/No/Abstain"), qsTr("Single Choice"), qsTr("Multiple Choice")]
                            KeyNavigation.tab: {
                                if (currentIndex === 0 || !choicesListView || choicesListView.count === 0) return backButton
                                return choicesListView.itemAtIndex(0).textEdit
                            }
                        }
                        Text {
                            id: choicesLabel
                            Layout.alignment: Qt.AlignTop
                            text: qsTr("Choices:")
                            color: MMPTheme.textColor
                            visible: responseTypeComboBox.currentIndex != 0
                        }
                        Rectangle {
                            id: choicesListRect
                            Layout.fillWidth: true
                            implicitHeight: Math.max(26+30*choicesListView.count, 26+3*30)
                            color: MMPTheme.themeSelect(MMPTheme.cWhite, "#17222c")
                            border.color: MMPTheme.lightBorderColor
                            radius: 4
                            visible: responseTypeComboBox.currentIndex != 0
                            ListView {
                                id: choicesListView
                                clip: true
                                currentIndex: 0
                                interactive: false
                                onCurrentItemChanged: if (currentItem) currentItem.textEdit.forceActiveFocus()
                                anchors {
                                    top: parent.top
                                    left: parent.left
                                    right: parent.right
                                    bottom: tableFooterRect.top
                                    leftMargin: 1
                                    rightMargin: 1
                                    topMargin: 1
                                }
                                model: ListModel{
                                    id: choicesListModel
                                    ListElement{choiceText: ""}
                                    ListElement{choiceText: ""}
                                }
                                delegate: MouseArea {
                                    width: choicesListView.width
                                    height: 30
                                    onClicked: choicesListView.currentIndex=index
                                    property Item textEdit: choiceTextEdit
                                    Rectangle {
                                        anchors.fill: parent
                                        color: choicesListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                                        radius: 4
                                        RowLayout {
                                            spacing: 10
                                            anchors {
                                                fill: parent
                                                leftMargin: 10
                                                rightMargin: 10
                                            }
                                            Text {
                                                id: choiceNumberLabel
                                                height: parent.height
                                                Layout.preferredWidth: 10
                                                Layout.alignment: Qt.AlignVCenter
                                                text: (index + 1) + "."
                                                color: MMPTheme.textColor
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            TextField {
                                                id: choiceTextEdit
                                                Layout.preferredHeight: parent.height - 5
                                                Layout.fillWidth: true
                                                Layout.alignment: Qt.AlignVCenter
                                                placeholderText: qsTr("Poll Answer")
                                                color: MMPTheme.textColor
                                                verticalAlignment: Text.AlignVCenter
                                                selectByMouse: true
                                                onFocusChanged: if (focus) choicesListView.currentIndex = index
                                                KeyNavigation.tab: {
                                                    if (index === choicesListView.count-1) { //Last item
                                                        return backButton
                                                    } else {
                                                        return choicesListView.itemAtIndex(index+1).textEdit
                                                    }
                                                }
                                                onTextEdited: choicesListView.model.set(index, {choiceText: text})
                                            }
                                        }
                                    }
                                }
                            }

                            TableFooter {
                                id: tableFooterRect
                                borderColor: MMPTheme.lightBorderColor
                                RowLayout {
                                    id: buttonRow
                                    height: parent.height
                                    spacing: 0
                                    Button {
                                        id: addChoiceButton
                                        Layout.fillHeight: true
                                        implicitWidth: 30
                                        icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_add_light.svg","/icons/buttons/ic_btn_add_dark.svg")
                                        background: Item{}
                                        onClicked: choicesListModel.append({})
                                    }
                                    Rectangle {
                                        id: buttonSeparator
                                        color: MMPTheme.lightBorderColor
                                        width: 1
                                        Layout.fillHeight: true
                                    }
                                    Button {
                                        id: removeChoiceButton
                                        Layout.fillHeight: true
                                        implicitWidth: 30
                                        icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_remove_light.svg","/icons/buttons/ic_btn_remove_dark.svg")
                                        background: Item{}
                                        onClicked: choicesListModel.remove(choicesListView.currentIndex)
                                        enabled: choicesListModel.count > 0
                                    }
                                    Rectangle {
                                        id: buttonSeparator2
                                        color: MMPTheme.lightBorderColor
                                        width: 1
                                        Layout.fillHeight: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Button {
                id: createPollButton
                text: qsTr("Create Poll")
                icon.source: MMPTheme.themeSelect("/icons/buttons/ic_btn_create_light.svg", "/icons/buttons/ic_btn_create_dark.svg")
                KeyNavigation.tab: durationSpinbox
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    margins: 10
                }
            }
            Button {
                id: backButton
                text: qsTr("Back")
                onClicked: stackView.pop()
                Keys.onReturnPressed: stackView.pop()
                Keys.onEnterPressed: stackView.pop()
                icon.source: MMPTheme.themeSelect("/icons/generic/ic_chevron_left_light.svg", "/icons/generic/ic_chevron_left_dark.svg")
                KeyNavigation.tab: createPollButton
                anchors {
                    bottom: parent.bottom
                    right: createPollButton.left
                    margins: 10
                }
            }
        }
    }

    Component {
        id: projectSelectionView
        Rectangle {
            id: projectBackground
            color: MMPTheme.backgroundColor
            property Item defaultFocusItem: addProjectRadioButton

            Rectangle {
                id: projectBody
                color: MMPTheme.bodyColor
                radius: 4
                clip: true
                anchors {
                    top: parent.top
                    bottom: nextButton.top
                    left: parent.left
                    right: parent.right
                    margins: 10
                }
                ScrollView {
                    id: contentScrollView
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: contentColumn.implicitHeight + 40
                    ColumnLayout {
                        id: contentColumn
                        spacing: 20
                        anchors {
                            top: parent.top
                            topMargin: 20
                            left: parent.left
                            right: parent.right
                            leftMargin: 30
                            rightMargin: 30
                        }
                        Text {
                            id: createPollTitleLabel
                            text: qsTr("Project Listing Proposal")
                            color: MMPTheme.textColor
                            font.weight: Font.DemiBold
                            font.pixelSize: 15
                            clip: true
                        }
                        Rectangle {
                            id: separator
                            height: 1
                            color: MMPTheme.separatorColor
                            Layout.fillWidth: true
                        }

                        RadioButton {
                            id: addProjectRadioButton
                            text: qsTr("Add an unlisted project")
                            KeyNavigation.tab: removeProjectRadioButton
                            Keys.onReturnPressed: checked = true
                            Keys.onEnterPressed: checked = true
                            focus: true
                        }
                        RadioButton {
                            id: removeProjectRadioButton
                            text: qsTr("Remove a listed project")
                            KeyNavigation.tab: projectNameTextField
                            Keys.onReturnPressed: checked = true
                            Keys.onEnterPressed: checked = true
                        }
                        Rectangle {
                            //                        id: separator
                            height: 1
                            color: MMPTheme.separatorColor
                            Layout.fillWidth: true
                        }
                        ColumnLayout {
                            id: addProjectColumnLayout
                            visible: addProjectRadioButton.checked
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? implicitHeight : 0    //Fixes a binding loop error
                            spacing: 20
                            Text {
                                id: proposalInfoText
                                color: MMPTheme.textColor
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                text: qsTr("Proposals must follow community guidelines for validation. Please review the wiki and verify that the prerequisites have been fulfilled:")
                            }
                            ClickableLink {
                                id: whitelistGuidelinesLink
                                urlString: "https://gridcoin.us/wiki/whitelist-process"
                                displayString: "gridcoin.us/wiki/whitelist-process"
                            }
                            Rectangle {
                                //id: separator
                                height: 1
                                color: MMPTheme.separatorColor
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                id: projectNameRow
                                spacing: 20
                                implicitHeight: projectNameTextField.implicitHeight
                                Text {
                                    id: projectNameLabel
                                    color: MMPTheme.textColor
                                    text: qsTr("Project Name:")
                                }
                                TextField {
                                    id: projectNameTextField
                                    KeyNavigation.tab: requirementConfirmationCheckbox
                                }
                            }
                            CheckBox {
                                id: requirementConfirmationCheckbox
                                text: qsTr("This project satisfies the Gridcoin listing criteria")
                                KeyNavigation.tab: backButton
                                Keys.onEnterPressed: toggle()
                                Keys.onReturnPressed: toggle()
                            }
                        }
                        ColumnLayout {
                            id: removeProjectColumnLayout
                            visible: removeProjectRadioButton.checked
                            Layout.preferredHeight: visible ? implicitHeight : 0    //Fixes a binding loop error
                            Layout.fillWidth: true
                            spacing: 20
                            Text {
                                id: chooseProjectLabel
                                text: qsTr("Choose a project to delist:")
                                color: MMPTheme.textColor
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: currentProjectsListView.implicitHeight + 2
                                Layout.minimumHeight: currentProjectsListView.implicitHeight + 2
                                Layout.maximumHeight: currentProjectsListView.implicitHeight + 2
                                border.color: MMPTheme.lightBorderColor
                                color: "transparent"
                                radius: 4
                                ListView {
                                    id: currentProjectsListView
                                    x:1
                                    y:1
                                    width: parent.width-2
                                    implicitHeight: count*25
                                    interactive: false
                                    model: ListModel {
                                        ListElement {projectName: "Universe@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                        ListElement {projectName: "Einstein@home"}
                                    }
                                    delegate: MouseArea {
                                        width: parent.width
                                        height: 25
                                        onClicked: currentProjectsListView.currentIndex=index
                                        property alias projectName: projectNameDataLabel.text
                                        Rectangle {
                                            anchors.fill: parent
                                            color: currentProjectsListView.currentIndex===index ? MMPTheme.themeSelect(MMPTheme.cFrostWhite, "#212c3b") : "transparent"
                                            radius: 4
                                            Text {
                                                id: projectNameDataLabel
                                                text: model.projectName
                                                color: MMPTheme.textColor
                                                verticalAlignment: Text.AlignVCenter
                                                anchors {
                                                    fill: parent
                                                    leftMargin: 20
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Button {
                id: nextButton
                text: qsTr("Next")
                icon.source: MMPTheme.themeSelect("/icons/generic/ic_chevron_right_light.svg", "/icons/generic/ic_chevron_right_dark.svg")
                KeyNavigation.tab: addProjectRadioButton
                onClicked: activate()
                Keys.onEnterPressed: activate()
                Keys.onReturnPressed: activate()
                enabled: {
                    if (addProjectRadioButton.checked) {
                        return requirementConfirmationCheckbox.checked && projectNameTextField.text != ""
                    }
                    else if (removeProjectRadioButton.checked) {
                        return true
                    } else {
                        return false
                    }
                }

                function activate() {
                    var text = "[Project Listing] "
                    if (addProjectRadioButton.checked) {
                        text += "Add " + projectNameTextField.text
                    }
                    else {
                        text += "Remove " + currentProjectsListView.currentItem.projectName
                    }

                    stackView.push(pollCreationDetailView, {pollTitle: text, pollTypeString: "Project Listing"})
                }
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    margins: 10
                }
            }
            Button {
                id: backButton
                text: qsTr("Back")
                onClicked: stackView.pop()
                Keys.onReturnPressed: stackView.pop()
                Keys.onEnterPressed: stackView.pop()
                icon.source: MMPTheme.themeSelect("/icons/generic/ic_chevron_left_light.svg", "/icons/generic/ic_chevron_left_dark.svg")
                KeyNavigation.tab: nextButton
                anchors {
                    bottom: parent.bottom
                    right: nextButton.left
                    margins: 10
                }
            }
        }
    }
}
