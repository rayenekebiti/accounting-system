import QtQuick
import QtQuick.Controls.Basic
import App

// Language switcher. Uses the `i18n` context property (LocaleController) — NOT
// `locale`, which every Item shadows with a built-in QLocale. Labels come from
// i18n.languages ({code, label native endonym, rtl}); selection + switching use
// the explicit code, never fragile implicit QLocale display behaviour.
Item {
    id: root
    objectName: "languageSwitcher"

    implicitWidth:  btn.implicitWidth
    implicitHeight: btn.implicitHeight

    IconButton {
        id: btn
        anchors.fill: parent
        content: "🌐"
        accessibleName: qsTr("Change language")
        onClicked: menu.open()
    }

    Menu {
        id: menu
        objectName: "langMenu"

        MenuItem {
            objectName:  "lang_en"
            text:        i18n.languages[0].label   // "English"
            checkable:   true
            checked:     i18n.language === "en"
            onTriggered: i18n.setLanguage("en")
        }

        MenuItem {
            objectName:  "lang_fr"
            text:        i18n.languages[1].label   // "Français"
            checkable:   true
            checked:     i18n.language === "fr"
            onTriggered: i18n.setLanguage("fr")
        }

        MenuItem {
            objectName:  "lang_ar"
            text:        i18n.languages[2].label   // "العربية"
            checkable:   true
            checked:     i18n.language === "ar"
            onTriggered: i18n.setLanguage("ar")
        }
    }
}
