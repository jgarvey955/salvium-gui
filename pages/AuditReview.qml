import QtQuick 2.9
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import QtQuick.Dialogs 1.2

Item {
    id: review
    property bool busy: false
    property string reportText: qsTr("Check your wallet, then export private evidence if verification is needed.")
    property string errorText: ""

    function stateText(state) {
        var labels = {
            "CLEARED": qsTr("Verified"),
            "CLEARED_BY_DEVELOPER": qsTr("Cleared by the main developer"),
            "DENIED_BY_DEVELOPER": qsTr("Denied by the main developer"),
            "BAD": qsTr("Malformed origin found"),
            "BAD_ORIGIN_FOUND": qsTr("Malformed origin found"),
            "REVIEW": qsTr("Awaiting verification"),
            "REVIEW_REQUIRED": qsTr("Developer review needed"),
            "AUDIT_PASSED": qsTr("Audit passed"),
            "MATURING": qsTr("Verified; ordinary locks still apply"),
            "EMPTY": qsTr("No supported outputs")
        };
        return labels[state] || state;
    }

    function reasonText(reason) {
        var labels = {
            "FUNDING_NOT_YET_VERIFIED": qsTr("Export a request for the main developer to verify."),
            "CONFIRMED_MALFORMED_ORIGIN": qsTr("The transaction created this asset without the required authorization."),
            "PROVEN_ILLICIT_FUNDING_REQUIRES_REVIEW": qsTr("Proven illicit funding requires a decision by the main developer."),
            "MISSING_FUNDING_EVIDENCE": qsTr("More funding evidence is needed; this does not establish misconduct."),
            "PUBLIC_VERIFICATION_WORK_LIMIT": qsTr("The verification limit was reached. More evidence or processing is needed."),
            "MISSING_PAYOUT_AUTHORIZATION_EVIDENCE": qsTr("Historical stake payout authorization needs review."),
            "VERIFIED_ISSUANCE": qsTr("Issuance verified."),
            "ACCEPTED_OPENING": qsTr("Covered by the accepted earlier audit."),
            "ALL_FUNDING_VERIFIED": qsTr("Every actual funding source was verified."),
            "ALL_RING_CANDIDATES_VERIFIED": qsTr("Every possible funding source was verified."),
            "PREMATURE_FUNDING": qsTr("Funding was used before its ordinary unlock height."),
            "INVALID_ISSUANCE_OR_TRANSACTION": qsTr("An issuance or transaction defect requires review.")
        };
        return labels[reason] || reason;
    }

    function run(action, file) {
        if (busy || !currentWallet) return;
        busy = true;
        errorText = "";
        currentWallet.auditReviewAsync(action, file || "", developer.text, snapshot.text, function(json, error) {
            busy = false;
            if (error) { errorText = error; return; }
            try {
                var report = JSON.parse(json);
                var lines = [stateText(report.state), qsTr("Every account and subaddress in this wallet")];
                var balances = report.balances || [];
                for (var i = 0; i < balances.length; ++i) {
                    var b = balances[i];
                    lines.push(b.asset_type + ": " + b.good_count + qsTr(" cleared, ") +
                        b.bad_count + qsTr(" denied or malformed, ") + b.unresolved_count + qsTr(" awaiting review"));
                }
                lines.push(qsTr("Stake principal: %1 cleared, %2 denied or malformed, %3 awaiting review")
                    .arg(report.stake_good_count).arg(report.stake_bad_count).arg(report.stake_unresolved_count));
                var outputs = report.outputs || [];
                var shown = 0;
                for (var j = 0; j < outputs.length; ++j) {
                    var o = outputs[j];
                    if (o.spent) continue;
                    if (++shown > 50) continue;
                    lines.push(o.transaction + ":" + o.output_index + (o.stake ? " (stake)" : "") +
                        "\n" + stateText(o.state) + " — " + reasonText(o.reason) + (o.decision_reason ? "\n" + o.decision_reason : ""));
                }
                if (shown > 50) lines.push(qsTr("Showing 50 of %1 checked unspent outputs.").arg(shown));
                reportText = lines.join("\n\n");
                if (action === "export") reportText = qsTr("Audit request saved. Share it privately with the main developer for processing.") + "\n\n" + reportText;
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
            text: qsTr("You initiate an audit by exporting a request. The main developer processes it: verified funding clears automatically and uncertain cases receive a CLEAR or DENY recommendation.")
        }
        TextField {
            id: snapshot
            Layout.fillWidth: true
            placeholderText: qsTr("Snapshot block from the audit coordinator, or leave blank for the current tip")
            enabled: !review.busy
        }
        RowLayout {
            Button { text: review.busy ? qsTr("Working…") : qsTr("Check wallet"); enabled: !review.busy; onClicked: review.run("review", "") }
            Button { text: qsTr("Request audit (export)"); enabled: !review.busy; onClicked: exportDialog.open() }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Exported evidence contains amounts and links to spends. It stays on your computer until you choose to share the file.")
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Audit clearance preserves ordinary spending rules and stake locks.")
        }
        TextField {
            id: developer
            Layout.fillWidth: true
            placeholderText: qsTr("Main developer public key obtained independently")
            enabled: !review.busy
        }
        Button {
            text: qsTr("Import signed result")
            enabled: !review.busy && /^[0-9a-fA-F]{64}$/.test(developer.text.trim())
            onClicked: importDialog.open()
        }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#d95b4d"; textFormat: Text.PlainText; text: review.errorText; visible: text.length > 0 }
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
