import QtQml 2.2

QtObject {
    enum ConnectionStatus {
        ConnectionStatus_Disconnected,
        ConnectionStatus_Connected,
        ConnectionStatus_WrongVersion,
        ConnectionStatus_Connecting
    }
}
