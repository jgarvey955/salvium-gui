import QtQuick 2.9
import QtTest 1.2
import "../../pages" as Pages

TestCase {
    id: testCase
    name: "StakingBalance"
    width: 800
    height: 700
    when: windowShown
    property bool isOpenGL: false
    property bool isMac: false

    QtObject {
        id: settings
        property string assetType: "SAL1"
        property bool useRemoteNode: true
    }
    QtObject {
        id: appWindow
        property var currentWallet: mockWallet
        property var persistentSettings: settings
        property bool viewOnly: false
        property bool daemonSynced: true
        property bool themeTransition: false
        property int walletMode: 2
    }
    property var currentWallet: appWindow.currentWallet
    property var persistentSettings: settings
    QtObject {
        id: translationManager
        property string emptyString: ""
    }
    QtObject {
        id: walletManager
        function displayAmount(value) { return (value / 100000000).toFixed(8); }
        function amountFromString(value) { return Math.round(Number(value) * 100000000); }
    }
    QtObject {
        id: mockWallet
        // Mutating the member emits no property notification, just like a
        // balance returned by the real C++ Q_INVOKABLE wallet method.
        property var amounts: ({ unlocked: 11295196510 })
        property bool viewOnly: false
        function unlockedBalance(asset, account) {
            testCase.compare(asset, "SAL1");
            testCase.compare(account, 0);
            return amounts.unlocked;
        }
        function connected() { return 1; }
        signal updated()
        signal refreshed()
        signal heightRefreshed(int walletHeight, int daemonHeight, int targetHeight)
        signal transactionCommitted(bool status, var transaction, var txids)
    }

    Component { id: stakingComponent; Pages.Staking {} }
    property var page

    function init() {
        mockWallet.amounts.unlocked = 11295196510;
        appWindow.currentWallet = mockWallet;
        appWindow.viewOnly = false;
        settings.assetType = "SAL1";
        page = createTemporaryObject(stakingComponent, testCase);
        verify(page !== null);
        page.onPageCompleted();
    }

    function test_balanceRefresh_data() {
        return [{tag: "updated"}, {tag: "refreshed"},
                {tag: "heightRefreshed"}, {tag: "transactionCommitted"}];
    }

    function test_balanceRefresh(data) {
        var label = findChild(page, "stakingSpendableBalance");
        var amount = findChild(page, "stakingAmountInput");
        var button = findChild(page, "stakingSubmitButton");
        compare(label.text.trim(), "112.95196510 SAL1");
        amount.text = "50";
        verify(button.enabled);

        mockWallet.amounts.unlocked = 199844413;
        if (data.tag === "heightRefreshed")
            mockWallet.heightRefreshed(577629, 577629, 577629);
        else if (data.tag === "transactionCommitted")
            mockWallet.transactionCommitted(true, null, []);
        else
            mockWallet[data.tag]();

        compare(label.text.trim(), "1.99844413 SAL1");
        verify(amount.error);
        verify(!button.enabled);
        verify(page.stakeButtonWarning.indexOf("unlocked balance") !== -1);

        amount.text = "1";
        verify(!amount.error);
        verify(button.enabled);
        amount.text = "0";
        verify(amount.error);
        verify(!button.enabled);
        appWindow.viewOnly = true;
        amount.text = "0.5";
        verify(!button.enabled);
    }

    function test_pageReentry() {
        mockWallet.amounts.unlocked = 199844413;
        page.onPageCompleted();
        compare(findChild(page, "stakingSpendableBalance").text.trim(), "1.99844413 SAL1");
    }

    function test_walletClosedAndReopened() {
        appWindow.currentWallet = null;
        compare(findChild(page, "stakingSpendableBalance").text.trim(), "?.?? SAL1");
        mockWallet.amounts.unlocked = 199844413;
        appWindow.currentWallet = mockWallet;
        compare(findChild(page, "stakingSpendableBalance").text.trim(), "1.99844413 SAL1");
    }

    function test_assetSelectionDoesNotChangeStakeAsset() {
        settings.assetType = "salCULT";
        mockWallet.amounts.unlocked = 199844413;
        mockWallet.updated();
        compare(findChild(page, "stakingSpendableBalance").text.trim(), "1.99844413 SAL1");
    }
}
