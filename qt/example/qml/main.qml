import QtQuick 2.2
import QtQuick.Controls 1.0
import QtQuick.Dialogs 1.1
import org.asexample 1.0

ApplicationWindow {
    title: qsTr("QAppStream Example")
    width: 640
    height: 480
    visible: true

    menuBar: MenuBar {
        Menu {
            title: qsTr("File")
            MenuItem {
                text: qsTr("Exit")
                onTriggered: Qt.quit();
            }
        }
    }

    AsIntf {
        id: asintf
    }

    MessageDialog {
        id: simpleMessageDialog
        title: "Meep"
        text: ""
        Component.onCompleted: visible = false
    }

    Button {
        text: qsTr("Test")
        anchors.centerIn: parent
        onClicked: {
            simpleMessageDialog.text = asintf.simple_test()
            simpleMessageDialog.visible = true
        }
    }
}
