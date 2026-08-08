import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// InvoiceEditor — ModalSheet-based invoice create/edit form.
// Bound to the `invoiceEditor` context property (InvoiceEditorViewModel).
//
// Keyboard workflow:
//   Open          → customer field gets focus
//   Tab/Shift+Tab → next/prev logical field
//   Return        → advances to next header field; in line cells advances cols;
//                   in the last cell of the last line → addBlankLine + focus new desc
//   Ctrl+S / Ctrl+Return → trySave()
//   Esc           → requestClose (guarded by dirty check)
ModalSheet {
    id: root

    objectName: "invoiceEditorRoot"

    // Emitted when the user has no customers yet and taps "Add customer" — Main opens the customer
    // editor over this sheet; on save, invoiceEditor.refreshCustomerOptions() repopulates the picker.
    signal addCustomerRequested()

    title: invoiceEditor.isNew
           ? qsTr("New Invoice")
           : qsTr("Edit %1").arg(invoiceEditor.invoiceNumber)

    // ── Touch tracking for progressive validation ────────────────────────────
    property bool customerTouched:  false
    property bool numberTouched:    false
    property bool issueTouched:     false
    property bool dueTouched:       false

    // Pending new-line focus: after addBlankLine we want to focus the new row's
    // description field. The Repeater delegate checks this index on completion.
    property int  pendingFocusRow:  -1

    // Inline save-error display
    property string saveError: ""

    function resetTouched() {
        customerTouched = false
        numberTouched   = false
        issueTouched    = false
        dueTouched      = false
        saveError       = ""
    }

    // Maps a VM error KEY → translated message. Empty until the field is touched
    // or showErrors is on. Reads i18n.language so the binding re-evaluates live on
    // language switch (qsTr behind a function call isn't caught by retranslate()).
    function fieldError(touched, key) {
        const lang = i18n.language   // dependency: re-run on language change
        if (key === "" || !(touched || invoiceEditor.showErrors))
            return ""
        switch (key) {
        case "required":       return qsTr("This field is required.")
        case "tooLong":        return qsTr("This value is too long.")
        case "invalidDate":    return qsTr("Enter a valid date (YYYY-MM-DD).")
        case "dueBeforeIssue": return qsTr("Due date must be on or after the issue date.")
        default:               return ""
        }
    }

    // Focus first header field on open; reset touched flags.
    onOpened: {
        resetTouched()
        customerField.forceActiveFocus()
    }

    // Dirty guard on close request.
    onRequestClose: {
        if (invoiceEditor.dirty)
            confirmDiscard.open()
        else
            root.close()
    }

    // ── Save / keyboard shortcuts ────────────────────────────────────────────
    function trySave() {
        invoiceEditor.commit()
        // On success:  invoiceEditor emits saved()  → Main closes + refreshes
        // On failure:  invoiceEditor emits validationFailed() → focus first bad field
    }

    Shortcut {
        sequences: ["Ctrl+S", "Ctrl+Return", "Ctrl+Enter"]
        context:   Qt.WindowShortcut
        onActivated: root.trySave()
    }

    // ── VM signal connections ────────────────────────────────────────────────
    Connections {
        target: invoiceEditor

        function onValidationFailed(field) {
            if      (field === "customerId")     customerField.forceActiveFocus()
            else if (field === "invoiceNumber")  numberField.forceActiveFocus()
            else if (field === "issueDate")      issueField.forceActiveFocus()
            else if (field === "dueDate")        dueField.forceActiveFocus()
        }

        function onSaveFailed(msg) {
            root.saveError = msg
        }
    }

    // ── Status options (qsTr re-evaluates on language change) ────────────────
    property var statusOptions: [
        { value: 0, label: qsTr("Draft")   },
        { value: 1, label: qsTr("Posted")  },
        { value: 2, label: qsTr("Paid")    },
        { value: 3, label: qsTr("Overdue") },
        { value: 4, label: qsTr("Void")    }
    ]

    // ════════════════════════════════════════════════════════════════════════
    // Body content (goes into ModalSheet's default content alias → bodyCol)
    // ════════════════════════════════════════════════════════════════════════

    // ── Section: Details ─────────────────────────────────────────────────────
    Text {
        text:           qsTr("Details")
        font.pixelSize: Theme.font.sm
        font.weight:    Theme.font.weightSemibold
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary
        Layout.fillWidth: true
    }

    GridLayout {
        Layout.fillWidth: true
        columns:         2
        columnSpacing:   Theme.space.lg
        rowSpacing:      Theme.space.md

        // Row 1: Customer | Invoice #
        Select {
            id: customerField
            objectName: "invCustomerSelect"   // for interaction tests (drive the real onActivated)
            Layout.fillWidth: true
            label:        qsTr("Customer")
            required:     true
            model:        invoiceEditor.customerOptions
            currentValue: invoiceEditor.customerId
            error:        root.fieldError(root.customerTouched, invoiceEditor.customerError)
            onActivated: (v) => {
                invoiceEditor.customerId = v   // property WRITE (setCustomerId is not a QML-callable method)
                root.customerTouched = true
            }
            // Tab → numberField
            KeyNavigation.tab: numberField
        }

        AppTextField {
            id: numberField
            Layout.fillWidth: true
            label:       qsTr("Invoice #")
            required:    true
            text:        invoiceEditor.invoiceNumber
            horizontalAlignment: Text.AlignLeft  // invoice number is an LTR identifier
            error:       root.fieldError(root.numberTouched, invoiceEditor.numberError)
            onEditingFinished: {
                invoiceEditor.invoiceNumber = text
                root.numberTouched = true
            }
            Keys.onReturnPressed: issueField.forceActiveFocus()
        }

        // Row 2: Issue date | Due date
        AppTextField {
            id: issueField
            Layout.fillWidth: true
            label:       qsTr("Issue date")
            required:    true
            placeholder: qsTr("YYYY-MM-DD")
            text:        invoiceEditor.issueDate
            error:       root.fieldError(root.issueTouched, invoiceEditor.issueDateError)
            onEditingFinished: {
                invoiceEditor.issueDate = text
                root.issueTouched = true
            }
            Keys.onReturnPressed: dueField.forceActiveFocus()
        }

        AppTextField {
            id: dueField
            Layout.fillWidth: true
            label:       qsTr("Due date")
            required:    true
            placeholder: qsTr("YYYY-MM-DD")
            text:        invoiceEditor.dueDate
            error:       root.fieldError(root.dueTouched, invoiceEditor.dueDateError)
            onEditingFinished: {
                invoiceEditor.dueDate = text
                root.dueTouched = true
            }
            Keys.onReturnPressed: statusField.forceActiveFocus()
        }

        // Row 3: Status (spans 1 col; leave second col empty for balance)
        Select {
            id: statusField
            Layout.fillWidth: true
            label:        qsTr("Status")
            model:        root.statusOptions
            currentValue: invoiceEditor.status
            onActivated:  (v) => invoiceEditor.status = v
        }

        // Spacer to fill second column
        Item { Layout.fillWidth: true }
    }

    // First-run helper: an invoice needs a customer, and the picker has no inline "add". Without
    // this, a brand-new user who taps "New Invoice" hits an empty required dropdown with no way
    // forward. Shown only while there are zero customers; the button opens the customer editor.
    RowLayout {
        Layout.fillWidth: true
        visible: invoiceEditor.customerOptions.length === 0
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            text: qsTr("You don't have any customers yet — add one to invoice them.")
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
            wrapMode: Text.WordWrap
        }
        AppButton {
            text:    qsTr("Add customer")
            variant: "secondary"
            accessibleName: qsTr("Add a customer, then invoice them")
            onClicked: root.addCustomerRequested()
        }
    }

    // ── Section: Line items ──────────────────────────────────────────────────
    Text {
        text:           qsTr("Line items")
        font.pixelSize: Theme.font.sm
        font.weight:    Theme.font.weightSemibold
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary
        Layout.fillWidth: true
        Layout.topMargin: Theme.space.sm
    }

    // Column-header row
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.sm

        Text {
            Layout.fillWidth:   true
            text:               qsTr("Description")
            font.pixelSize:     Theme.font.xs
            font.family:        Theme.font.uiFamily
            color:              Theme.color.textSecondary
        }
        Text {
            Layout.preferredWidth: 64
            text:                  qsTr("Qty")
            font.pixelSize:        Theme.font.xs
            font.family:           Theme.font.uiFamily
            color:                 Theme.color.textSecondary
            horizontalAlignment:   Text.AlignRight  // numeric column: right-aligned for scanning (mirror-flips to inline-end in RTL)
        }
        Text {
            Layout.preferredWidth: 96
            text:                  qsTr("Unit Price")
            font.pixelSize:        Theme.font.xs
            font.family:           Theme.font.uiFamily
            color:                 Theme.color.textSecondary
            horizontalAlignment:   Text.AlignRight  // numeric column: right-aligned for scanning (mirror-flips to inline-end in RTL)
        }
        Text {
            Layout.preferredWidth: 64
            text:                  qsTr("Tax %")
            font.pixelSize:        Theme.font.xs
            font.family:           Theme.font.uiFamily
            color:                 Theme.color.textSecondary
            horizontalAlignment:   Text.AlignRight  // numeric column: right-aligned for scanning (mirror-flips to inline-end in RTL)
        }
        Text {
            Layout.preferredWidth: 96
            text:                  qsTr("Amount")
            font.pixelSize:        Theme.font.xs
            font.family:           Theme.font.uiFamily
            color:                 Theme.color.textSecondary
            horizontalAlignment:   Text.AlignRight  // numeric column: right-aligned for scanning (mirror-flips to inline-end in RTL)
        }
        // Spacer for the ✕ remove-button column
        Item { implicitWidth: 32 }
    }

    // Line rows via Repeater
    Column {
        id: linesColumn
        Layout.fillWidth: true
        spacing: Theme.space.xs

        Repeater {
            id: linesRepeater
            model: invoiceEditor.lines

            delegate: RowLayout {
                id: lineRow

                required property int    index
                required property string description
                required property string qtyText
                required property string unitPriceText
                required property string taxText
                required property string lineTotalText

                width:   linesColumn.width
                spacing: Theme.space.sm

                // Focus this row's description if it's the pending new row
                Component.onCompleted: {
                    if (lineRow.index === root.pendingFocusRow) {
                        descField.forceActiveFocus()
                        root.pendingFocusRow = -1
                    }
                }

                FieldInput {
                    id: descField
                    Layout.fillWidth: true
                    text:            lineRow.description
                    placeholderText: qsTr("Description")
                    onEditingFinished: invoiceEditor.lines.setCell(lineRow.index, "description", text)
                    // Return → qty
                    Keys.onReturnPressed: qtyField.forceActiveFocus()
                }

                FieldInput {
                    id: qtyField
                    objectName: "lineQty" + lineRow.index   // for interaction tests
                    Layout.preferredWidth: 64
                    horizontalAlignment:   Text.AlignRight  // numeric column: right-aligned for scanning (mirror-flips to inline-end in RTL)
                    inputMethodHints:      Qt.ImhFormattedNumbersOnly
                    text:                  lineRow.qtyText
                    onEditingFinished:     invoiceEditor.lines.setCell(lineRow.index, "qtyText", text)
                    Keys.onReturnPressed:  priceField.forceActiveFocus()
                }

                FieldInput {
                    id: priceField
                    Layout.preferredWidth: 96
                    horizontalAlignment:   Text.AlignRight  // numeric column: right-aligned for scanning (mirror-flips to inline-end in RTL)
                    inputMethodHints:      Qt.ImhFormattedNumbersOnly
                    text:                  lineRow.unitPriceText
                    onEditingFinished:     invoiceEditor.lines.setCell(lineRow.index, "unitPriceText", text)
                    Keys.onReturnPressed:  taxField.forceActiveFocus()
                }

                FieldInput {
                    id: taxField
                    Layout.preferredWidth: 64
                    horizontalAlignment:   Text.AlignRight  // numeric column: right-aligned for scanning (mirror-flips to inline-end in RTL)
                    inputMethodHints:      Qt.ImhFormattedNumbersOnly
                    text:                  lineRow.taxText
                    onEditingFinished:     invoiceEditor.lines.setCell(lineRow.index, "taxText", text)
                    // Return in last cell of last line → add blank line + focus new desc.
                    // Return in last cell of non-last line → Tab to next line's desc.
                    Keys.onReturnPressed: {
                        const lineCount = invoiceEditor.lines.count
                        if (lineRow.index === lineCount - 1) {
                            root.pendingFocusRow = lineRow.index + 1
                            invoiceEditor.lines.addBlankLine()
                        } else {
                            // advance focus: remove-button → next-line-desc via tab chain
                            removeBtn.forceActiveFocus()
                            removeBtn.KeyNavigation.tab = linesRepeater.itemAt(lineRow.index + 1)
                        }
                    }
                }

                CurrencyAmount {
                    Layout.preferredWidth: 96
                    amount: lineRow.lineTotalText
                }

                IconButton {
                    id:        removeBtn
                    content:   "✕"
                    accessibleName: qsTr("Remove line %1").arg(lineRow.index + 1)
                    onClicked: invoiceEditor.lines.removeLine(lineRow.index)
                }
            }
        }
    }

    // Add line button
    AppButton {
        variant:  "ghost"
        text:     qsTr("＋ Add line")
        onClicked: {
            root.pendingFocusRow = invoiceEditor.lines.count
            invoiceEditor.lines.addBlankLine()
        }
    }

    // Lines validation error (VM exposes the key "linesRequired"; message here)
    Text {
        visible:          invoiceEditor.showErrors && invoiceEditor.linesError.length > 0
        text:             qsTr("Add at least one line item with a description, quantity, and price.")
        color:            Theme.color.expense
        font.pixelSize:   Theme.font.xs
        font.family:      Theme.font.uiFamily
        wrapMode:         Text.WordWrap
        Layout.fillWidth: true
    }

    // ── Section: Totals ──────────────────────────────────────────────────────
    Divider { Layout.fillWidth: true }

    ColumnLayout {
        Layout.alignment: Qt.AlignRight
        spacing: Theme.space.xs

        // Subtotal row
        RowLayout {
            spacing: Theme.space.lg

            Text {
                text:           qsTr("Subtotal")
                font.pixelSize: Theme.font.base
                font.family:    Theme.font.uiFamily
                color:          Theme.color.textSecondary
            }
            Item { implicitWidth: Theme.space.xl }
            CurrencyAmount {
                Layout.preferredWidth: 120
                amount: invoiceEditor.subtotalText
            }
        }

        // Tax row
        RowLayout {
            spacing: Theme.space.lg

            Text {
                text:           qsTr("Tax")
                font.pixelSize: Theme.font.base
                font.family:    Theme.font.uiFamily
                color:          Theme.color.textSecondary
            }
            Item { implicitWidth: Theme.space.xl }
            CurrencyAmount {
                Layout.preferredWidth: 120
                amount: invoiceEditor.taxText
            }
        }

        // Total row (bold)
        RowLayout {
            spacing: Theme.space.lg

            Text {
                text:           qsTr("Total")
                font.pixelSize: Theme.font.base
                font.weight:    Theme.font.weightBold
                font.family:    Theme.font.uiFamily
                color:          Theme.color.textPrimary
            }
            Item { implicitWidth: Theme.space.xl }
            CurrencyAmount {
                id: totalAmount
                Layout.preferredWidth: 120
                amount: invoiceEditor.totalText
                font.weight: Theme.font.weightBold
            }
        }
    }

    // Save error display (e.g. storage failure)
    Text {
        visible:          root.saveError.length > 0
        text:             root.saveError
        color:            Theme.color.expense
        font.pixelSize:   Theme.font.sm
        font.family:      Theme.font.uiFamily
        wrapMode:         Text.WordWrap
        Layout.fillWidth: true
    }

    // ════════════════════════════════════════════════════════════════════════
    // Footer
    // ════════════════════════════════════════════════════════════════════════
    footerData: [
        Text {
            visible:        invoiceEditor.dirty
            text:           qsTr("Unsaved changes")
            color:          Theme.color.textSecondary
            font.pixelSize: Theme.font.xs
            font.family:    Theme.font.uiFamily
        },
        Item { Layout.fillWidth: true },
        AppButton {
            // Deliver this invoice to the customer as CSV (only meaningful once it has an id).
            visible: !invoiceEditor.isNew
            text:    qsTr("Export CSV")
            variant: "secondary"
            onClicked: { exportVm.exportInvoice(invoiceEditor.editId); exportVm.openExportsFolder() }
        },
        AppButton {
            // Professional printable PDF in the current UI language (EN/FR/AR, RTL-aware).
            visible: !invoiceEditor.isNew
            text:    qsTr("Export PDF")
            variant: "secondary"
            onClicked: { exportVm.exportInvoicePdf(invoiceEditor.editId, i18n.language); exportVm.openExportsFolder() }
        },
        AppButton {
            text:    qsTr("Cancel")
            variant: "ghost"
            onClicked: root.requestClose()
        },
        AppButton {
            id:      saveBtn
            text:    qsTr("Save")
            variant: "primary"
            onClicked: root.trySave()
        }
    ]

    // ── Discard confirm dialog ───────────────────────────────────────────────
    ConfirmDialog {
        id:             confirmDiscard
        title:          qsTr("Discard changes?")
        message:        qsTr("Your unsaved changes will be lost.")
        confirmText:    qsTr("Discard")
        cancelText:     qsTr("Keep editing")
        confirmVariant: "danger"

        onConfirmed: {
            invoiceEditor.discard()
            root.close()
        }
        onCancelled: { /* keep editor open */ }
    }
}
