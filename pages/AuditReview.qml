import QtQuick 2.9
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import QtQuick.Dialogs 1.2

Item {
    id: review
    property bool busy: false
    property string reportText: qsTr("Check your wallet, then export private evidence if verification is needed.")
    property string errorText: ""

    function run(action, file) {
        if (busy || !currentWallet) return;
        busy = true;
        errorText = "";
        currentWallet.auditReviewAsync(action, file || "", developer.text, snapshot.text, function(json, error) {
            busy = false;
            if (error) { errorText = error; return; }
            try {
                var report = JSON.parse(json);
                var lines = [report.state, qsTr("Every account and subaddress in this wallet")];
                var balances = report.balances || [];
                for (var i = 0; i < balances.length; ++i) {
                    var b = balances[i];
                    lines.push(b.asset_type + ": " + b.good_count + qsTr(" cleared, ") +
                        b.bad_count + qsTr(" denied or malformed, ") + b.unresolved_count + qsTr(" awaiting review"));
                }
                var outputs = report.outputs || [];
                var shown = 0;
                for (var j = 0; j < outputs.length; ++j) {
                    var o = outputs[j];
                    if (o.spent) continue;
                    if (++shown > 50) continue;
                    lines.push(o.transaction + ":" + o.output_index + (o.stake ? " (stake)" : "") +
                        "\n" + o.state + " — " + o.reason + (o.decision_reason ? "\n" + o.decision_reason : ""));
                }
                if (shown > 50) lines.push(qsTr("Showing 50 of %1 checked unspent outputs.").arg(shown));
                reportText = lines.join("\n\n");
                if (action === "export") reportText = qsTr("Private evidence saved. Share it only with your chosen reviewer.") + "\n\n" + reportText;
            } catch (e) { errorText = qsTr("The wallet returned an invalid audit report."); }
        });
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        Label { text: qsTr("Wallet audit"); font.pixelSize: 24 }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Verified funding clears automatically during review. Uncertain cases receive a developer recommendation. Audit results preserve ordinary spending and stake locks.")
        }
        TextField {
            id: snapshot
            Layout.fillWidth: true
            placeholderText: qsTr("Snapshot block from the audit coordinator, or leave blank for the current tip")
            enabled: !review.busy
        }
        RowLayout {
            Button { text: review.busy ? qsTr("Working…") : qsTr("Check wallet"); enabled: !review.busy; onClicked: review.run("review", "") }
            Button { text: qsTr("Export private evidence"); enabled: !review.busy; onClicked: exportDialog.open() }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Exported evidence contains amounts and links to spends. It stays on your computer until you choose to share the file.")
        }
        TextField {
            id: developer
            Layout.fillWidth: true
            placeholderText: qsTr("Developer public key obtained independently")
            enabled: !review.busy
        }
        Button {
            text: qsTr("Import signed result")
            enabled: !review.busy && /^[0-9a-fA-F]{64}$/.test(developer.text.trim())
            onClicked: importDialog.open()
        }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#d95b4d"; text: review.errorText; visible: text.length > 0 }
        TextArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            text: review.reportText
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.WrapAnywhere
        }
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Save private audit evidence to a new file")
        selectExisting: false
        nameFilters: [qsTr("Audit evidence (*.json)")]
        onAccepted: review.run("export", fileUrl.toString())
    }
    FileDialog {
        id: importDialog
        title: qsTr("Open developer signed audit result")
        selectExisting: true
        nameFilters: [qsTr("Audit result (*.json)")]
        onAccepted: review.run("import", fileUrl.toString())
    }
}
