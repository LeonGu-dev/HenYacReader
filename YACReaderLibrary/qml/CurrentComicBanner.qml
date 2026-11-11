import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: banner
    objectName: "currentComicBanner"
    property var infoObject: null
    property int sourceIndex: -1
    property url coverSource
    property bool sectionVisible: true
    property color surfaceColor: "transparent"
    readonly property bool narrow: width < 560
    signal openRequested()
    implicitHeight: sectionVisible ? layout.implicitHeight + 40 : 20
    height: implicitHeight

    // The outer grid handles scrolling; metadata must never be clipped to a fixed height.
    function handlesWheelAt(position) { return false; }

    Rectangle {
        objectName: "bannerSurface"
        anchors.fill: parent
        visible: banner.sectionVisible
        color: banner.surfaceColor
        Rectangle {
            objectName: "bannerDivider"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Qt.rgba(infoTextColor.r, infoTextColor.g, infoTextColor.b, 0.16)
        }
    }

    GridLayout {
        id: layout
        visible: banner.sectionVisible
        x: 20
        y: 20
        width: Math.max(0, Math.min(860, banner.width - 40))
        columns: banner.narrow ? 1 : 2
        columnSpacing: 24
        rowSpacing: 20

        ColumnLayout {
            Layout.preferredWidth: banner.narrow ? layout.width : 170
            Layout.minimumWidth: banner.narrow ? layout.width : 170
            Layout.maximumWidth: banner.narrow ? layout.width : 170
            Layout.alignment: Qt.AlignTop
            spacing: 12
            Image {
                objectName: "bannerCover"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: banner.narrow ? 180 : 230
                source: banner.coverSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                asynchronous: true
            }
            Button {
                objectName: "bannerContinueReading"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 170
                Layout.maximumWidth: layout.width
                text: qsTr("Continue reading")
                enabled: banner.infoObject ? banner.infoObject.available : false
                onClicked: banner.openRequested()
                padding: 8
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 14
                    color: buttonTextColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 5
                    color: buttonColor
                    opacity: parent.down ? 0.8 : 1
                    border.width: parent.activeFocus ? 1 : 0
                    border.color: infoTextColor
                }
            }
        }

        ComicInfoView {
            objectName: "bannerDetails"
            Layout.fillWidth: true
            Layout.preferredWidth: banner.narrow ? layout.width : Math.max(0, layout.width - 194)
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignTop
            infoObject: banner.infoObject
            sourceIndex: banner.sourceIndex
            compact: true
        }
    }
}
