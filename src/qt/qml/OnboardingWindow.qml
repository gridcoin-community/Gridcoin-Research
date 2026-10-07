/*
    A setup window presented to new users guiding them with the new user experience.
*/
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import MMPTheme 1.0
import QtQml

Window {
    id: window
    signal onboardingFinished
    property int chosenRole: soloRole
    //Chosen Role Enumeration
    readonly property int soloRole: 0
    readonly property int poolRole: 1
    readonly property int stakerRole: 2

    flags: Qt.FramelessWindowHint
    height: 640
    width: 720
    visible: true
    color: "transparent"
    NumberAnimation {
        id: opacityAnimation
        target: window
        properties: "opacity"
        duration: 1000
        from: 1
        to: 0
        running: false
        onFinished: closeOnboardingWindow()
    }
    function closeOnboardingWindow() {
        onboardingFinished()
        window.close()
    }
    Rectangle {
        id: background
        anchors.fill: parent
        border.color: "transparent"
        radius: 4
        LinearGradient {
            id: backgroundGradient
            anchors.fill: parent
            source: background
            cached: true
            start: Qt.point(0, 0)
            end: Qt.point(width, height)
            gradient: Gradient {
                GradientStop { position: 0.1; color: "#c76dd7" }
                GradientStop { position: 1.0; color: "#3324ae" }
            }
        }
        StackView {
            id: stackView
            initialItem: welcomeView
            anchors.fill: parent
        }
    }

    Component {
        id: welcomeView
        Item {
            Image {
                id: logoImage
                source: "qrc:/icons/logos/ic_logo_app_gradient_white.svg"
                sourceSize: Qt.size(120, 120)
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 40
                }
            }
            ColumnLayout {
                id: welcomeInfoColumnLayout
                spacing: 20
                anchors {
                    top: logoImage.bottom
                    left: parent.left
                    right: parent.right
                    margins: 50
                    topMargin: 30
                }
                Text {
                    id: welcomeText
                    text: qsTr("Welcome to Gridcoin!")
                    color: MMPTheme.cWhite
                    font.pixelSize: 24
                    font.weight: Font.Medium
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    color: MMPTheme.cWhite
                    text: qsTr("The next few screens will guide you through the setup of your Gridcoin wallet.")
                    font.pixelSize: 15
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    id: gridcoinInfoText
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.6)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    text: qsTr(
                              "Gridcoin was offically launched in 2013. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam quis velit in arcu ullamcorper placerat. Nullam mattis tempor mi, vitae suscipit sapien. Phasellus convallis porta ante vel ullamcorper. Fusce lobortis fermentum dolor non gravida. Ut placerat lacus ac ante venenatis ornare.

Morbi nec sollicitudin leo. In hac habitasse platea dictumst. Nunc id ultricies dui, sed porta purus. Phasellus condimentum feugiat orci id blandit. Phasellus vitae mi sit amet lacus facilisis fringilla vel non nisi. Proin et diam vel orci dictum scelerisque. Ut ultricies elit quis dolor condimentum volutpat. Sed a nisl nulla. Mauris tincidunt ultricies mi, quis pulvinar purus rutrum eget. Duis porta erat a tincidunt pretium.

Etiam convallis lectus magna, quis scelerisque augue dapibus sit amet. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer et est porttitor, tempor sem et, varius sapien. Etiam faucibus justo non sollicitudin gravida. In in ullamcorper eros, et cursus odio. Proin non fermentum nibh.
")
                }
            }
            Item {
                id: buttonRow
                height: exitButton.implicitHeight
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    margins: 40
                    bottomMargin: 30
                }
                OnboardingButton {
                    id: exitButton
                    text: qsTr("Exit")
                    onPressed: Qt.quit()
                    anchors.left: parent.left
                }
                OnboardingPageIndicator {
                    currentPage: 1
                    anchors.centerIn: parent
                }

                OnboardingButton {
                    id: continueButton
                    text: qsTr("Continue")
                    anchors.right: parent.right
                    focus: true
                    onPressed: stackView.push(participationView)
                }
            }
        }
    }
    Component {
        id: participationView
        Item {
            Image {
                id: logoImage
                source: "qrc:/icons/logos/ic_logo_app_gradient_white.svg"
                sourceSize: Qt.size(120, 120)
                Layout.alignment: Qt.AlignHCenter
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 40
                }
            }
            Text {
                id: participateText
                text: qsTr("How would you like to participate?")
                color: MMPTheme.cWhite
                font.pixelSize: 24
                font.weight: Font.Medium
                anchors {
                    top: logoImage.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 30
                }
            }
            Item {
                id: participationChoiceItem
                height: soloMouseArea.height + soloRadioButton.anchors.topMargin + soloRadioButton.height
                anchors {
                    top: participateText.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: 40
                    leftMargin: 40
                    rightMargin: 40
                }
                MouseArea {
                    id: soloMouseArea
                    onPressed: soloRadioButton.checked=true
                    anchors.left: parent.left
                    height: 180
                    width: 180
                    Circle {
                        id: soloImageBackground
                        radius: 90
                        color: MMPTheme.translucent(MMPTheme.cWhite, soloRadioButton.checked ? 0.3 :0.2)
                        anchors.centerIn: parent
                        border.color: soloRadioButton.checked ? "#8e12fc" : "transparent"
                        border.width: 10
                        Circle {
                            radius: 60
                            color: MMPTheme.translucent(MMPTheme.cWhite, soloRadioButton.checked ? 0.3 :0.2)
                            anchors.centerIn: parent
                            Image {
                                id: soloImage
                                anchors.centerIn: parent
                                sourceSize: Qt.size(80, 80)
                                source: soloRadioButton.checked ? "qrc:/icons/onboarding/iс_solo_active.svg" : "qrc:/icons/onboarding/iс_solo_inactive.svg"
                            }
                        }
                    }
                }
                RadioButton {
                    id: soloRadioButton
                    text: qsTr("Solo")
                    anchors {
                        top: soloMouseArea.bottom
                        horizontalCenter: soloMouseArea.horizontalCenter
                        topMargin: 10
                    }
                    contentItem: Text {
                        leftPadding: soloRadioButton.indicator.width + soloRadioButton.spacing
                        text: soloRadioButton.text
                        color: MMPTheme.cWhite
                        font.pixelSize: 16
                    }
                }
                MouseArea {
                    id: poolMouseArea
                    onPressed: poolRadioButton.checked=true
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: 180
                    width: 180
                    Circle {
                        id: poolImageBackground
                        radius: 90
                        color: MMPTheme.translucent(MMPTheme.cWhite, poolRadioButton.checked ? 0.3 :0.2)
                        anchors.centerIn: parent
                        border.color: poolRadioButton.checked ? "#8e12fc" : "transparent"
                        border.width: 10
                        Circle {
                            radius: 60
                            color: MMPTheme.translucent(MMPTheme.cWhite, poolRadioButton.checked ? 0.3 :0.2)
                            anchors.centerIn: parent
                            Image {
                                id: poolImage
                                anchors.centerIn: parent
                                sourceSize: Qt.size(80, 80)
                                source: poolRadioButton.checked ? "qrc:/icons/onboarding/iс_pool_active.svg" : "qrc:/icons/onboarding/iс_pool_inactive.svg"
                            }
                        }
                    }
                }
                RadioButton {
                    id: poolRadioButton
                    text: qsTr("Pool")
                    anchors {
                        top: poolMouseArea.bottom
                        horizontalCenter: poolMouseArea.horizontalCenter
                        topMargin: 10
                    }
                    contentItem: Text {
                        leftPadding: poolRadioButton.indicator.width + poolRadioButton.spacing
                        text: poolRadioButton.text
                        color: MMPTheme.cWhite
                        font.pixelSize: 16
                    }
                }
                MouseArea {
                    id: stakerMouseArea
                    onPressed: stakerRadioButton.checked=true
                    anchors.right: parent.right
                    height: 180
                    width: 180
                    Circle {
                        id: stakerImageBackground
                        radius: 90
                        color: MMPTheme.translucent(MMPTheme.cWhite, stakerRadioButton.checked ? 0.3 :0.2)
                        anchors.centerIn: parent
                        border.color: stakerRadioButton.checked ? "#8e12fc" : "transparent"
                        border.width: 10
                        Circle {
                            radius: 60
                            color: MMPTheme.translucent(MMPTheme.cWhite, stakerRadioButton.checked ? 0.3 :0.2)
                            anchors.centerIn: parent
                            Image {
                                id: stakerImage
                                anchors.centerIn: parent
                                sourceSize: Qt.size(80, 80)
                                source: stakerRadioButton.checked ? "qrc:/icons/onboarding/ic_non_cruncher_active.svg" : "qrc:/icons/onboarding/ic_non_cruncher_inactive.svg"
                            }
                        }
                    }
                }
                RadioButton {
                    id: stakerRadioButton
                    text: qsTr("Staker")
                    anchors {
                        top: stakerMouseArea.bottom
                        horizontalCenter: stakerMouseArea.horizontalCenter
                        topMargin: 10
                    }
                    contentItem: Text {
                        leftPadding: stakerRadioButton.indicator.width + stakerRadioButton.spacing
                        text: stakerRadioButton.text
                        color: MMPTheme.cWhite
                        font.pixelSize: 16
                    }
                }
            }
            Item {
                id: buttonRow
                height: backButton.implicitHeight
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    margins: 40
                    bottomMargin: 30
                }
                OnboardingButton {
                    id: backButton
                    text: qsTr("Back")
                    onPressed: stackView.pop()
                    anchors.left: parent.left
                }
                OnboardingPageIndicator {
                    currentPage: 2
                    anchors.centerIn: parent
                }

                OnboardingButton {
                    id: continueButton
                    text: qsTr("Continue")
                    anchors.right: parent.right
                    focus: true
                    enabled: soloRadioButton.checked || poolRadioButton.checked || stakerRadioButton.checked
                    onPressed: {
                        if (soloRadioButton.checked) {
                            chosenRole = soloRole
                            stackView.push(noBoincView)
                        }
                        if (poolRadioButton.checked) {
                            chosenRole = poolRole
                            stackView.push(noBoincView)
                        }
                        if (stakerRadioButton.checked) {
                            chosenRole = stakerRole
                            stackView.push(stakerAlmostThereView)
                        }
                    }
                }
            }
        }
    }
    Component {
        id: noBoincView
        Item {
            Image {
                id: boincMissingImage
                source: "qrc:/icons/onboarding/img_onboarding_noboinc.svg"
                sourceSize: Qt.size(160, 160)
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 40
                }
            }
            Text {
                id: boincRequiredText
                text: qsTr("BOINC installation required.")
                color: MMPTheme.cWhite
                font.pixelSize: 28
                font.weight: Font.Medium
                anchors {
                    top: boincMissingImage.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 30
                }
            }

            Rectangle {
                id: boincPathBackground
                width: 400
                height: boincPathInfoText.implicitHeight + 20
                color: MMPTheme.translucent(MMPTheme.cWhite, 0.3)
                border.color: MMPTheme.translucent(MMPTheme.cWhite, 0.6)
                radius: 4
                anchors {
                    top: boincRequiredText.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 30
                }
                Button {
                    id: boincPathButton
                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: 10
                    }
                    background: Circle {
                        radius: 12
                        implicitWidth: 2*radius
                        implicitHeight: 2*radius
                        color: MMPTheme.translucent(MMPTheme.cWhite, boincPathButton.hovered ? 0.5 : 0.3)
                        border.color: MMPTheme.translucent(MMPTheme.cWhite, boincPathButton.hovered ? 0.8 : 0.6)
                    }
                    contentItem: Image {
                        sourceSize: Qt.size(width, height)
                        source: "qrc:/icons/onboarding/ic_onboarding_btn_more.svg"
                        anchors.centerIn: parent
                    }
                }
                Text {
                    id: boincPathInfoText
                    color: MMPTheme.cWhite
                    wrapMode: Text.WordWrap
                    text: qsTr("If you installed the BOINC client in a non-default location, choose the path and click <b>Try Again</b>. Otherwise follow the steps below.")
                    font.pixelSize: 14
                    anchors {
                        verticalCenter: parent.verticalCenter
                        left: parent.left
                        right: boincPathButton.left
                        margins: 10
                    }
                }
            }

            GridLayout {
                id: stepsGridLayout
                columns: 2
                columnSpacing: 15
                rowSpacing: 10
                anchors {
                    top: boincPathBackground.bottom
                    topMargin: 20
                    left: parent.left
                    leftMargin: 210
                }
                Circle {
                    radius: 15
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.3)
                    border.color: MMPTheme.translucent(MMPTheme.cWhite, 0.6)
                    Text {
                        text: qsTr("%L1").arg(1)
                        color: MMPTheme.cWhite
                        font.pixelSize: 15
                        anchors.centerIn: parent
                    }
                }
                Text {
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.8)
                    text: qsTr("Navigate to the BOINC website")
                    font.pixelSize: 15
                }
                Circle {
                    radius: 15
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.3)
                    border.color: MMPTheme.translucent(MMPTheme.cWhite, 0.6)
                    Text {
                        text: qsTr("%L1").arg(2)
                        color: MMPTheme.cWhite
                        font.pixelSize: 15
                        anchors.centerIn: parent
                    }
                }
                Text {
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.8)
                    text: qsTr("Download the BOINC client")
                    font.pixelSize: 15
                }
                Circle {
                    radius: 15
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.3)
                    border.color: MMPTheme.translucent(MMPTheme.cWhite, 0.6)
                    Text {
                        text: qsTr("%L1").arg(3)
                        color: MMPTheme.cWhite
                        font.pixelSize: 15
                        anchors.centerIn: parent
                    }
                }
                Text {
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.8)
                    text: qsTr("Install the client on your device")
                    font.pixelSize: 15
                }
                Circle {
                    radius: 15
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.3)
                    border.color: MMPTheme.translucent(MMPTheme.cWhite, 0.6)
                    Text {
                        text: qsTr("%L1").arg(4)
                        color: MMPTheme.cWhite
                        font.pixelSize: 15
                        anchors.centerIn: parent
                    }
                }
                Text {
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.8)
                    text: qsTr("Click <b>Try Again</b> to check for the installation")
                    font.pixelSize: 15
                }
            }

            Item {
                id: buttonRow
                height: backButton.implicitHeight
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    margins: 40
                    bottomMargin: 30
                }
                OnboardingButton {
                    id: backButton
                    text: qsTr("Back")
                    onPressed: stackView.pop()
                    anchors.left: parent.left
                }
                OnboardingPageIndicator {
                    currentPage: 3
                    anchors.centerIn: parent
                }

                OnboardingButton {
                    id: continueButton
                    text: qsTr("Try Again")
                    anchors.right: parent.right
                    focus: true
                    onPressed: {
                        if (chosenRole === soloRole) {
                            stackView.replace(boincEmailView, StackView.Immediate)
                        } else {
                            stackView.replace(boincPoolSuccessView, StackView.Immediate)
                        }
                    }
                }
            }
        }
    }
    Component {
        id: boincEmailView
        Item {
            Image {
                id: boincImage
                source: "qrc:/icons/onboarding/img_onboarding_boinc.svg"
                sourceSize: Qt.size(160, 160)
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 40
                }
            }
            Text {
                id: almostDoneText
                text: qsTr("You are almost done.")
                color: MMPTheme.cWhite
                font.pixelSize: 28
                font.weight: Font.Medium
                anchors {
                    top: boincImage.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 30
                }
            }
            Text {
                id: finishText
                text: qsTr("Enter your BOINC email address:")
                color: MMPTheme.cWhite
                font.pixelSize: 14
                anchors {
                    top: almostDoneText.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 50
                }
            }

            TextField {
                id: emailTextField
                color: MMPTheme.cWhite
                font.pixelSize: 14
                placeholderText: qsTr("Email")
                placeholderTextColor: MMPTheme.translucent(MMPTheme.cWhite, 0.5)
                anchors {
                    top: finishText.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 20
                }
                background: Rectangle {
                    implicitWidth: 300
                    implicitHeight: 36
                    radius: 4
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.2)
                    border.color: MMPTheme.translucent(MMPTheme.cWhite, emailTextField.activeFocus ? 1 : 0.5)
                }
            }

            Item {
                id: buttonRow
                height: backButton.implicitHeight
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    margins: 40
                    bottomMargin: 30
                }
                OnboardingButton {
                    id: backButton
                    text: qsTr("Back")
                    onPressed: stackView.pop()
                    anchors.left: parent.left
                }
                OnboardingPageIndicator {
                    currentPage: 3
                    anchors.centerIn: parent
                }

                OnboardingButton {
                    id: continueButton
                    text: qsTr("Finish")
                    anchors.right: parent.right
                    focus: true
                    enabled: emailTextField.text !== ""
                    onPressed: stackView.push(blockchainLoadingView)
                }
            }
        }
    }
    Component {
        id: boincPoolSuccessView
        Item {
            Image {
                id: boincImage
                source: "qrc:/icons/onboarding/img_onboarding_boinc.svg"
                sourceSize: Qt.size(160, 160)
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 40
                }
            }
            Text {
                id: almostDoneText
                text: qsTr("You are almost done.")
                color: MMPTheme.cWhite
                font.pixelSize: 28
                font.weight: Font.Medium
                anchors {
                    top: boincImage.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 30
                }
            }
            Text {
                id: finishText
                text: qsTr("Click <b>Finish</b> to start syning your wallet")
                color: MMPTheme.cWhite
                font.pixelSize: 14
                opacity: 0.7
                anchors {
                    top: almostDoneText.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 20
                }
            }

            Item {
                id: buttonRow
                height: backButton.implicitHeight
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    margins: 40
                    bottomMargin: 30
                }
                OnboardingButton {
                    id: backButton
                    text: qsTr("Back")
                    onPressed: stackView.pop()
                    anchors.left: parent.left
                }
                OnboardingPageIndicator {
                    currentPage: 3
                    anchors.centerIn: parent
                }

                OnboardingButton {
                    id: continueButton
                    text: qsTr("Finish")
                    anchors.right: parent.right
                    focus: true
                    onPressed: stackView.push(blockchainLoadingView)
                }
            }
        }
    }
    Component {
        id: stakerAlmostThereView
        Item {
            Image {
                id: plotImage
                source: "qrc:/icons/onboarding/img_onboarding_non_cruncher.svg"
                sourceSize: Qt.size(160, 160)
                anchors {
                    top: parent.top
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 40
                }
            }
            Text {
                id: almostDoneText
                text: qsTr("You are almost done.")
                color: MMPTheme.cWhite
                font.pixelSize: 28
                font.weight: Font.Medium
                anchors {
                    top: plotImage.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 30
                }
            }
            Text {
                id: finishText
                text: qsTr("Click <b>Finish</b> to start syning your wallet")
                color: MMPTheme.cWhite
                font.pixelSize: 14
                opacity: 0.7
                anchors {
                    top: almostDoneText.bottom
                    horizontalCenter: parent.horizontalCenter
                    topMargin: 20
                }
            }

            Item {
                id: buttonRow
                height: backButton.implicitHeight
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    margins: 40
                    bottomMargin: 30
                }
                OnboardingButton {
                    id: backButton
                    text: qsTr("Back")
                    onPressed: stackView.pop()
                    anchors.left: parent.left
                }
                OnboardingPageIndicator {
                    currentPage: 3
                    anchors.centerIn: parent
                }

                OnboardingButton {
                    id: continueButton
                    text: qsTr("Finish")
                    anchors.right: parent.right
                    focus: true
                    onPressed: stackView.push(blockchainLoadingView)
                }
            }
        }
    }
    Component {
        id: blockchainLoadingView
        Item {
            property int currentBlocksLoaded: 0
            property int totalBlocks: 2000000
            Timer {
                interval: 5
                repeat: true
                running: true
                onTriggered: {
                    currentBlocksLoaded += 2000
                    if (currentBlocksLoaded >= totalBlocks) {
                        running=false
                        if (chosenRole===soloRole) stackView.replace(null, soloMinerCompleteView)
                        if (chosenRole===poolRole) stackView.replace(null, poolMinerCompleteView)
                        if (chosenRole===stakerRole) stackView.replace(null, stakerCompleteView)
                    }
                }
            }

            ScrollView {
                id: scrollView
                clip: true
                contentWidth: availableWidth
                contentHeight: informationColumn.implicitHeight + 2
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    bottom: blockProgressBar.top
                    margins: 20
                    leftMargin: 20
                    rightMargin: 20
                }
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical: ScrollBar {
                    //TODO For some reason this generates two scroll bars, one default, small and unfunctioning. Bug has been difficult to track down
                    id: scrollBar
                    parent: scrollView.parent
                    policy: ScrollBar.AsNeeded
                    anchors {
                        top: scrollView.top
                        left: scrollView.right
                        bottom: scrollView.bottom
                    }
                    contentItem: Rectangle {
                        implicitHeight: 100
                        implicitWidth: 8
                        radius: width/2
                        color: MMPTheme.translucent(MMPTheme.cWhite, scrollBar.pressed ? 1 : 0.7)
                    }
                }

                ColumnLayout {
                    id: informationColumn
                    width: parent.width
                    spacing: 10
                    Text {
                        text: qsTr("You must sync the blockchain before proceeding.")
                        color: MMPTheme.cWhite
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        color: MMPTheme.cWhite
                        opacity: 0.7
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        text: qsTr("Gridcoin was offically launched in 2013. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam quis velit in arcu ullamcorper placerat. Nullam mattis tempor mi, vitae suscipit sapien. Phasellus convallis porta ante vel ullamcorper. Fusce lobortis fermentum dolor non gravida. Ut placerat lacus ac ante venenatis ornare.

Morbi nec sollicitudin leo. In hac habitasse platea dictumst. Nunc id ultricies dui, sed porta purus. Phasellus condimentum feugiat orci id blandit. Phasellus vitae mi sit amet lacus facilisis fringilla vel non nisi. Proin et diam vel orci dictum scelerisque. Ut ultricies elit quis dolor condimentum volutpat. Sed a nisl nulla. Mauris tincidunt ultricies mi, quis pulvinar purus rutrum eget. Duis porta erat a tincidunt pretium.

Etiam convallis lectus magna, quis scelerisque augue dapibus sit amet. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer et est porttitor, tempor sem et, varius sapien. Etiam faucibus justo non sollicitudin gravida. In in ullamcorper eros, et cursus odio. Proin non fermentum nibh.
")
                    }
                    Text {
                        text: qsTr("Acquire Gridcoin")
                        color: MMPTheme.cWhite
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        color: MMPTheme.cWhite
                        opacity: 0.7
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        text: qsTr("Gridcoin was offically launched in 2013. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam quis velit in arcu ullamcorper placerat. Nullam mattis tempor mi, vitae suscipit sapien. Phasellus convallis porta ante vel ullamcorper. Fusce lobortis fermentum dolor non gravida. Ut placerat lacus ac ante venenatis ornare.

Morbi nec sollicitudin leo. In hac habitasse platea dictumst. Nunc id ultricies dui, sed porta purus. Phasellus condimentum feugiat orci id blandit. Phasellus vitae mi sit amet lacus facilisis fringilla vel non nisi. Proin et diam vel orci dictum scelerisque. Ut ultricies elit quis dolor condimentum volutpat. Sed a nisl nulla. Mauris tincidunt ultricies mi, quis pulvinar purus rutrum eget. Duis porta erat a tincidunt pretium.

Etiam convallis lectus magna, quis scelerisque augue dapibus sit amet. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer et est porttitor, tempor sem et, varius sapien. Etiam faucibus justo non sollicitudin gravida. In in ullamcorper eros, et cursus odio. Proin non fermentum nibh.
")
                    }
                    Text {
                        color: MMPTheme.cWhite
                        opacity: 0.7
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        text: qsTr("Gridcoin was offically launched in 2013. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam quis velit in arcu ullamcorper placerat. Nullam mattis tempor mi, vitae suscipit sapien. Phasellus convallis porta ante vel ullamcorper. Fusce lobortis fermentum dolor non gravida. Ut placerat lacus ac ante venenatis ornare.

Morbi nec sollicitudin leo. In hac habitasse platea dictumst. Nunc id ultricies dui, sed porta purus. Phasellus condimentum feugiat orci id blandit. Phasellus vitae mi sit amet lacus facilisis fringilla vel non nisi. Proin et diam vel orci dictum scelerisque. Ut ultricies elit quis dolor condimentum volutpat. Sed a nisl nulla. Mauris tincidunt ultricies mi, quis pulvinar purus rutrum eget. Duis porta erat a tincidunt pretium.

Etiam convallis lectus magna, quis scelerisque augue dapibus sit amet. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer et est porttitor, tempor sem et, varius sapien. Etiam faucibus justo non sollicitudin gravida. In in ullamcorper eros, et cursus odio. Proin non fermentum nibh.")
                    }
                }
            }
            ProgressBar {
                id: blockProgressBar
                value: currentBlocksLoaded/totalBlocks
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: blocksLoadedText.top
                    margins: 20
                    bottomMargin: 10
                }
                background: Rectangle {
                    id: barBackgroundRect
                    implicitWidth: 200
                    implicitHeight: 6
                    radius: height/2
                    color: MMPTheme.translucent(MMPTheme.cWhite, 0.1)
                }
                contentItem: Item {
                    Rectangle {
                        height: parent.height
                        width: blockProgressBar.visualPosition * parent.width
                        radius: height/2
                        gradient: Gradient{
                            orientation: Gradient.Horizontal
                            GradientStop {
                                position: 0.0
                                color: "#00dbde"
                            }
                            GradientStop {
                                position: 1.0
                                color: {
                                    var startRed = 0
                                    var startGreen = 219
                                    var startBlue = 222
                                    var endRed = 252
                                    var endGreen = 0
                                    var endBlue = 255
                                    var outRed = blockProgressBar.value * endRed + (1-blockProgressBar.value) * startRed
                                    var outGreen = blockProgressBar.value * endGreen + (1-blockProgressBar.value) * startGreen
                                    var outBlue = blockProgressBar.value * endBlue + (1-blockProgressBar.value) * startBlue
                                    return Qt.rgba(outRed/255, outGreen/255, outBlue/255, 1)
                                }
                            }
                        }
                    }
                }
            }
            Text {
                id: blocksLoadedText
                text: qsTr("%L1/%L2 Blocks Loaded").arg(currentBlocksLoaded).arg(totalBlocks)
                color: MMPTheme.translucent(MMPTheme.cWhite, 0.7)
                font.pixelSize: 10
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    margins: 10
                    leftMargin: 20
                }
            }
            Text {
                id: timeRemainingText
                color: MMPTheme.translucent(MMPTheme.cWhite, 0.7)
                font.pixelSize: 10
                text: qsTr("%1 remaining").arg("2 hours")
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    rightMargin: 20
                    bottomMargin: 10
                }
            }
            Image {
                id: timeRemainingIcon
                source: "qrc:/icons/onboarding/ic_onboarding_time.svg"
                sourceSize: Qt.size(14, 14)
                anchors {
                    right: timeRemainingText.left
                    rightMargin: 5
                    verticalCenter: timeRemainingText.verticalCenter
                }
            }
        }
    }
    Component {
        id: soloMinerCompleteView
        Item {
            Image {
                id: backgroundStarsImage
                source: "qrc:/icons/onboarding/bg_onboarding_stars.svg"
                sourceSize: Qt.size(width, height)
                anchors.fill: parent
                Text {
                    id: completeText
                    text: qsTr("Congratulations, Solo Miner!")
                    color: MMPTheme.cWhite
                    font.pixelSize: 24
                    font.weight: Font.Medium
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.top
                        topMargin: 80
                    }
                }
                Text {
                    id: subtitleText
                    text: qsTr("You have successfully set up your wallet in solo mining mode")
                    color: MMPTheme.cWhite
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    opacity: 0.7
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: completeText.bottom
                        topMargin: 20
                    }
                }
                Image {
                    id: astronautImage
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_solo.svg"
                    sourceSize: Qt.size(1.2 * 248, 1.2 * 202)   // (248, 202) is base aspect ratio
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: subtitleText.bottom
                        topMargin: 100
                    }
                }
                OnboardingButton {
                    id: openWalletButton
                    text: qsTr("Open Wallet")
                    onPressed: opacityAnimation.start()
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 30
                    }
                }
            }
        }
    }
    Component {
        id: poolMinerCompleteView
        Item {
            Image {
                id: backgroundStarsImage
                source: "qrc:/icons/onboarding/bg_onboarding_stars.svg"
                sourceSize: Qt.size(width, height)
                anchors.fill: parent
                Text {
                    id: completeText
                    text: qsTr("Congratulations, Pool Miner!")
                    color: MMPTheme.cWhite
                    font.pixelSize: 24
                    font.weight: Font.Medium
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.top
                        topMargin: 80
                    }
                }
                Text {
                    id: subtitleText
                    text: qsTr("You have successfully set up your wallet in pool mining mode")
                    color: MMPTheme.cWhite
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    opacity: 0.7
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: completeText.bottom
                        topMargin: 20
                    }
                }
                Image {
                    id: planetImage
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_pool.svg"
                    sourceSize: Qt.size(320, 320)   // (320, 320) is base aspect ratio
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: subtitleText.bottom
                        topMargin: 70
                    }
                }
                OnboardingButton {
                    id: openWalletButton
                    text: qsTr("Open Wallet")
                    onPressed: opacityAnimation.start()
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 30
                    }
                }
            }
        }
    }
    Component {
        id: stakerCompleteView
        Item {
            Image {
                id: backgroundStarsImage
                source: "qrc:/icons/onboarding/bg_onboarding_stars.svg"
                sourceSize: Qt.size(width, height)
                anchors.fill: parent
                Text {
                    id: completeText
                    text: qsTr("Congratulations, Staker!")
                    color: MMPTheme.cWhite
                    font.pixelSize: 24
                    font.weight: Font.Medium
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.top
                        topMargin: 80
                    }
                }
                Text {
                    id: subtitleText
                    text: qsTr("You have successfully set up your wallet in staking mode")
                    color: MMPTheme.cWhite
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    opacity: 0.7
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: completeText.bottom
                        topMargin: 20
                    }
                }
                Image {
                    id: spaceshipImage
                    z: 10
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_non_cruncher.svg"
                    sourceSize: Qt.size(120, 156)   // (120, 156) is base aspect ratio
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: subtitleText.bottom
                        topMargin: 100
                    }
                }
                Image {
                    id: trailImage1
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_non_cruncher_ln1.svg"
                    sourceSize: Qt.size(parent.width, parent.height-y)
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: spaceshipImage.bottom
                        topMargin: -20
                    }
                }
                Image {
                    id: trailImage2
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_non_cruncher_ln2.svg"
                    sourceSize: Qt.size(parent.width, parent.height-y)
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: spaceshipImage.bottom
                        topMargin: -20
                    }
                }
                Image {
                    id: trailImage3
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_non_cruncher_ln3.svg"
                    sourceSize: Qt.size(parent.width, parent.height-y)
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: spaceshipImage.bottom
                        topMargin: -20
                    }
                }
                Image {
                    id: cloudImage1
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_non_cruncher_shp1.svg"
                    sourceSize: Qt.size(132, 57)   // (132, 57) is base aspect ratio
                    anchors {
                        right: spaceshipImage.left
                        rightMargin: -5
                        verticalCenter: spaceshipImage.verticalCenter
                    }
                }
                Image {
                    id: cloudImage2
                    source: "qrc:/icons/onboarding/img_onboarding_congrats_non_cruncher_shp2.svg"
                    sourceSize: Qt.size(132, 57)   // (132, 57) is base aspect ratio
                    anchors {
                        bottom: spaceshipImage.top
                        left: spaceshipImage.right
                        leftMargin: -20
                        bottomMargin: 0
                    }
                }
                OnboardingButton {
                    id: openWalletButton
                    text: qsTr("Open Wallet")
                    onPressed: opacityAnimation.start()
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        bottomMargin: 30
                    }
                }
            }
        }
    }
}
