import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: mainContainer
    objectName: "comicInfoDetails"
    implicitHeight: content.implicitHeight + 2 * inset
    height: implicitHeight

    property var infoObject: comicInfo
    property int sourceIndex: comic_info_index
    property bool compact: false
    property bool showFileInformation: !compact
    property var expandedFields: ({})
    readonly property int inset: compact ? 0 : (width < 450 ? 18 : 28)
    readonly property var rows: {
        let result = mainContainer.infoObject ? mainContainer.infoObject.detailRows.slice() : [];
        return compact ? result.sort((a, b) => a.compactOrder - b.compactOrder) : result;
    }
    readonly property var badges: mainContainer.infoObject ? mainContainer.infoObject.statusBadges : []
    readonly property var file: mainContainer.infoObject ? mainContainer.infoObject.fileDetails : ({})
    property bool fileExpanded: false
    property int colorRevision: 0
    readonly property real labelWidth: {
        let longest = 58;
        for (let row of rows)
            longest = Math.max(longest, Math.ceil(labelMetrics.advanceWidth(row.label)));
        return Math.min(width * 0.28, longest);
    }
    FontMetrics { id: labelMetrics; font.pixelSize: 14 }

    onFileChanged: { fileExpanded = false; expandedFields = ({}); }
    Connections {
        target: metadataChipColors
        function onColorsChanged() { mainContainer.colorRevision++; }
    }

    ColumnLayout {
        id: content
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: mainContainer.inset
        spacing: mainContainer.compact ? 12 : 16

        RowLayout {
            Layout.fillWidth: !mainContainer.compact
            Layout.preferredWidth: mainContainer.compact ? Math.min(content.width, titleColumn.implicitWidth + 36) : -1
            Layout.maximumWidth: content.width
            Layout.alignment: Qt.AlignLeft
            spacing: 16
            ColumnLayout {
                id: titleColumn
                Layout.fillWidth: true
                spacing: 4
                Text {
                    objectName: "detailTitle"
                    Layout.fillWidth: true
                    text: mainContainer.infoObject ? mainContainer.infoObject.getTitleOrFileName() : ""
                    textFormat: Text.PlainText
                    color: infoTextColor
                    font.pixelSize: 22
                    font.bold: true
                    wrapMode: Text.Wrap
                }
                Text {
                    objectName: "detailTranslatedTitle"
                    Layout.fillWidth: true
                    text: mainContainer.infoObject ? mainContainer.infoObject.metadata.translatedTitle ?? "" : ""
                    visible: text.length > 0
                    textFormat: Text.PlainText
                    color: infoMetadataTextColor
                    font.pixelSize: 15
                    wrapMode: Text.Wrap
                }
            }
            InfoFavorites {
                objectName: "detailFavorite"
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 4
                active: mainContainer.infoObject ? mainContainer.infoObject.isFavorite : false
                enabled: mainContainer.infoObject ? mainContainer.infoObject.available : false
                onActiveChangedByUser: function(active) {
                    // The model refreshes the current object after the write.
                    // Do not access mainContainer.infoObject after the helper returns.
                    if (active)
                        comicInfoHelper.addToFavorites(mainContainer.sourceIndex);
                    else
                        comicInfoHelper.removeFromFavorites(mainContainer.sourceIndex);
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 14
            InfoRating {
                objectName: "detailRating"
                height: 28
                rating: mainContainer.infoObject ? mainContainer.infoObject.rating : 0
                enabled: mainContainer.infoObject ? mainContainer.infoObject.canSetReadState : false
                onRatingChangedByUser: function(rating) {
                    comicInfoHelper.rate(mainContainer.sourceIndex, rating === mainContainer.infoObject.rating ? 0 : rating);
                }
            }
            Button {
                objectName: "detailRead"
                height: 28
                text: mainContainer.infoObject && mainContainer.infoObject.read ? qsTr("Read") : qsTr("Unread")
                enabled: mainContainer.infoObject ? mainContainer.infoObject.canSetReadState : false
                onClicked: comicInfoHelper.setRead(mainContainer.sourceIndex, !mainContainer.infoObject.read)
                contentItem: Text {
                    text: parent.text
                    color: infoTextColor
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.down ? Qt.rgba(infoTextColor.r, infoTextColor.g, infoTextColor.b, 0.12) : "transparent"
                    border.color: Qt.rgba(infoTextColor.r, infoTextColor.g, infoTextColor.b, 0.2)
                    radius: 4
                }
            }
            Text {
                objectName: "detailProgress"
                height: 28
                verticalAlignment: Text.AlignVCenter
                color: infoMetadataTextColor
                font.pixelSize: 14
                visible: mainContainer.infoObject ? mainContainer.infoObject.canSetReadState && Number(mainContainer.infoObject.numPages) > 0 : false
                text: !mainContainer.infoObject ? "" : mainContainer.infoObject.hasBeenOpened || mainContainer.infoObject.currentPage > 0
                      ? qsTr("Page %1 / %2").arg(Math.max(0, Math.min(mainContainer.infoObject.currentPage, Number(mainContainer.infoObject.numPages)))).arg(mainContainer.infoObject.numPages)
                      : qsTr("Not started · %1 pages").arg(mainContainer.infoObject.numPages)
            }
        }

        Flow {
            id: fieldsFlow
            Layout.fillWidth: true
            spacing: 12
            Repeater {
                model: mainContainer.rows
                delegate: RowLayout {
                    id: fieldRow
                    required property var modelData
                    objectName: "detailRow_" + modelData.key
                    width: mainContainer.compact && fieldsFlow.width >= 520 && modelData.compactHalf
                           ? (fieldsFlow.width - fieldsFlow.spacing) / 2 : fieldsFlow.width
                    spacing: 16
                    Text {
                        Layout.preferredWidth: mainContainer.labelWidth
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 4
                        text: fieldRow.modelData.label
                        textFormat: Text.PlainText
                        color: infoMetadataTextColor
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 3
                        Flow {
                            id: valueFlow
                            Layout.fillWidth: true
                            visible: fieldRow.modelData.chipField.length > 0
                            spacing: 7
                            Repeater {
                                model: fieldRow.modelData.chipField.length > 0
                                       ? (mainContainer.compact && fieldRow.modelData.compactLimit > 0 && !mainContainer.expandedFields[fieldRow.modelData.key]
                                          ? fieldRow.modelData.values.slice(0, fieldRow.modelData.compactLimit) : fieldRow.modelData.values) : []
                                delegate: Rectangle {
                                    id: chip
                                    required property string modelData
                                    readonly property var colors: {
                                        mainContainer.colorRevision;
                                        return metadataChipColors.colorsForField(fieldRow.modelData.chipField, modelData);
                                    }
                                    width: Math.min(valueFlow.width, Math.ceil(chipText.implicitWidth) + 24)
                                    height: chipText.contentHeight + 10
                                    color: colors.background
                                    border.color: Qt.rgba(0, 0, 0, 0.1)
                                    radius: 7
                                    Text {
                                        id: chipText
                                        width: Math.max(0, parent.width - 24)
                                        anchors.centerIn: parent
                                        text: chip.modelData
                                        textFormat: Text.PlainText
                                        color: chip.colors.foreground
                                        font.pixelSize: 14
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                        Button {
                            objectName: "detailTagsToggle"
                            visible: mainContainer.compact && fieldRow.modelData.compactLimit > 0 && fieldRow.modelData.values.length > fieldRow.modelData.compactLimit
                            text: mainContainer.expandedFields[fieldRow.modelData.key] ? qsTr("Collapse") : qsTr("+%1 more").arg(fieldRow.modelData.values.length - fieldRow.modelData.compactLimit)
                            onClicked: {
                                let fields = Object.assign({}, mainContainer.expandedFields);
                                fields[fieldRow.modelData.key] = !fields[fieldRow.modelData.key];
                                mainContainer.expandedFields = fields;
                            }
                            flat: true
                            padding: 0
                            contentItem: Text {
                                text: parent.text
                                color: infoMetadataTextColor
                                font.pixelSize: 13
                            }
                            background: Item { }
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: fieldRow.modelData.chipField.length === 0
                            text: fieldRow.modelData.values.join("、")
                            textFormat: Text.PlainText
                            color: infoTextColor
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: fieldRow.modelData.note
                            textFormat: Text.PlainText
                            color: infoMetadataTextColor
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: mainContainer.rows.length === 0 && mainContainer.badges.length === 0
            text: qsTr("No comic information entered")
            color: infoMetadataTextColor
            font.pixelSize: 14
            wrapMode: Text.Wrap
        }

        Flow {
            Layout.fillWidth: true
            visible: mainContainer.badges.length > 0
            spacing: 8
            Repeater {
                model: mainContainer.badges
                delegate: Rectangle {
                    required property string modelData
                    width: Math.min(parent.width, statusText.implicitWidth + 18)
                    height: statusText.contentHeight + 8
                    radius: 4
                    color: Qt.rgba(infoTextColor.r, infoTextColor.g, infoTextColor.b, 0.07)
                    Text {
                        id: statusText
                        width: Math.max(0, parent.width - 18)
                        anchors.centerIn: parent
                        text: parent.modelData
                        textFormat: Text.PlainText
                        font.pixelSize: 14
                        color: infoTextColor
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        Rectangle {
            visible: mainContainer.showFileInformation
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(infoTextColor.r, infoTextColor.g, infoTextColor.b, 0.15)
        }
        Button {
            objectName: "detailFileToggle"
            visible: mainContainer.showFileInformation
            Layout.fillWidth: true
            text: (mainContainer.fileExpanded ? "▾  " : "▸  ") + qsTr("File information")
                  + (Number(mainContainer.file.pages) > 0 ? " · " + qsTr("%1 pages").arg(mainContainer.file.pages) : "")
                  + (mainContainer.file.size ? " · " + mainContainer.file.size : "")
            onClicked: mainContainer.fileExpanded = !mainContainer.fileExpanded
            background: Item { }
            padding: 0
            contentItem: Text {
                text: parent.text
                textFormat: Text.PlainText
                color: infoMetadataTextColor
                font.pixelSize: 14
                wrapMode: Text.Wrap
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: mainContainer.showFileInformation && mainContainer.fileExpanded
            spacing: 6
            Repeater {
                model: [ { label: qsTr("Filename"), value: mainContainer.file.name ?? "" },
                         { label: qsTr("Location"), value: mainContainer.file.path ?? "" } ]
                delegate: Text {
                    required property var modelData
                    Layout.fillWidth: true
                    visible: modelData.value.length > 0
                    text: modelData.label + ": " + modelData.value
                    textFormat: Text.PlainText
                    color: infoMetadataTextColor
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
