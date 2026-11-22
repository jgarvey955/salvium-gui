import QtQuick 2.9
import QtTest 1.2
import "../../pages" as Pages

TestCase {
    id: testCase
    name: "YieldSnapshot"
    width: 800
    height: 800
    when: windowShown
    property bool isOpenGL: false
    property bool isMac: false
    QtObject {
        id: settings
        property bool useRemoteNode: true
    }
    QtObject {
        id: appWindow
        property var currentWallet: mockWallet
        property int height: 800
        property bool themeTransition: false
        property string accountsDir: ""
    }
    property var currentWallet: appWindow.currentWallet
    property var persistentSettings: settings
    QtObject {
        id: translationManager
        property string emptyString: ""
    }
    QtObject {
        id: walletManager
        function localDaemonSynced() { return true; }
    }
    Component {
        id: snapshotFactory
        QtObject {
            property int status: mockWallet.fail ? 1 : 0
            property string errorString: "Yield unavailable"
            property string burntFormatted: "90071992.54740993"
            property string lockedFormatted: "1.00000000"
            property string yieldFormatted: "0.00000011"
            property string period: "00:00:40:00"
            property int blockchain_height: mockWallet.height
            property string payouts: mockWallet.payouts
            function dispose() { mockWallet.disposed++; }
        }
    }
    QtObject {
        id: mockWallet
        property int height: 120
        property bool fail: false
        property int disposed: 0
        property string payouts: JSON.stringify([{
            blockheight: 100, hash: "transaction", asset_type: "SAL1",
            burnt: "9007199254740993", yield: "11",
            burntFormatted: "90071992.54740993", yieldFormatted: "0.00000011",
            isComplete: false, maturityHeight: "121"
        }])
        function getYieldInfo() { return snapshotFactory.createObject(testCase); }
        function blockchainHeight() { return height; }
        signal refreshed()
    }
    Component { id: pageFactory; Pages.Yield {} }
    property var page

    function init() {
        mockWallet.fail = false;
        mockWallet.height = 120;
        mockWallet.disposed = 0;
        page = createTemporaryObject(pageFactory, testCase);
        verify(page !== null);
        page.onPageCompleted();
    }
    function test_onePayoutPreservesAssetAndExactAmounts() {
        compare(page.txModelData.length, 1);
        compare(page.txModelData[0].asset_type, "SAL1");
        compare(page.txModelData[0].burnt, "9007199254740993");
        compare(page.txModelData[0].burntFormatted, "90071992.54740993");
        compare(page.txModelData[0].maturityHeight, "121");
        verify(!page.txModelData[0].isComplete);
        compare(mockWallet.disposed, 1);
    }
    function test_errorClearsPreviousResults() {
        mockWallet.fail = true;
        page.update();
        compare(page.txModelData.length, 0);
        compare(page.yieldError, "Yield unavailable");
        compare(mockWallet.disposed, 2);
    }
    function test_refreshReloadsSnapshotAfterWalletEvent() {
        mockWallet.height = 121;
        mockWallet.refreshed();
        tryCompare(page, "yieldHeight", 121);
        compare(mockWallet.disposed, 2);
    }
}
