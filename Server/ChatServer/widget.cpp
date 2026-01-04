#include "widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    this->setWindowTitle("TCP 通訊伺服器");
    this->resize(450, 350);

    setupUI(); // ui佈局

    tcpServer = new QTcpServer(this);

    // 連結啟動與關閉按鈕
    connect(startBtn, &QPushButton::clicked, this, &Widget::startServer);
    connect(stopBtn, &QPushButton::clicked, this, &Widget::stopServer);

    // 處理連線訊號
    connect(tcpServer, &QTcpServer::newConnection, this, &Widget::onNewConnection);
}

Widget::~Widget() {
    if (tcpServer->isListening()) stopServer();
}

void Widget::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *settingLayout = new QHBoxLayout();

    ipEdit = new QLineEdit("127.0.0.1");
    portEdit = new QLineEdit("8888");
    startBtn = new QPushButton("啟動伺服器");
    stopBtn = new QPushButton("關閉伺服器");
    stopBtn->setEnabled(false); //停用

    logEdit = new QTextEdit();
    logEdit->setReadOnly(true);

    settingLayout->addWidget(new QLabel("IP:"));
    settingLayout->addWidget(ipEdit);
    settingLayout->addWidget(new QLabel("Port:"));
    settingLayout->addWidget(portEdit);
    settingLayout->addWidget(startBtn);
    settingLayout->addWidget(stopBtn);

    mainLayout->addLayout(settingLayout);
    mainLayout->addWidget(new QLabel("伺服器日誌:"));
    mainLayout->addWidget(logEdit);
}

//啟動伺服器
void Widget::startServer() {
    QString ip = ipEdit->text();
    quint16 port = portEdit->text().toUShort();

    if (tcpServer->listen(QHostAddress(ip), port)) {
        logEdit->append(QString("伺服器成功啟動於 %1:%2").arg(ip).arg(port));
        startBtn->setEnabled(false);
        stopBtn->setEnabled(true);
        ipEdit->setReadOnly(true);
        portEdit->setReadOnly(true);
    } else {
        logEdit->append("錯誤: 無法啟動伺服器，請檢查 IP/Port。");
    }
}

//關閉伺服器
void Widget::stopServer() {
    // 中斷所有連線
    for (QTcpSocket *socket : clientList.values()) {
        socket->disconnectFromHost();
        socket->close();
    }
    clientList.clear();

    // 2. 停止監聽
    tcpServer->close();

    // 3.恢復介面狀態
    logEdit->append("伺服器已關閉。");
    startBtn->setEnabled(true);
    stopBtn->setEnabled(false);
    ipEdit->setReadOnly(false);
    portEdit->setReadOnly(false);
}

//處理新連線
void Widget::onNewConnection() {
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();

    // 連結訊號
    connect(clientSocket, &QTcpSocket::readyRead, this, &Widget::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &Widget::onDisconnected);

    logEdit->append("新連線已進入，等待登入訊息...");
}

// 接收與轉發訊息
void Widget::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    QString type = obj["type"].toString();

    // 處理登入
    if (type == "login") {
        QString nickname = obj["nickname"].toString();
        clientList[nickname] = socket;
        logEdit->append(QString("使用者登入: %1").arg(nickname));
        updateUserList(); // 廣播最新名單
    }
    // 處理聊天與檔案轉發
    else if (type == "chat") {
        QString target = obj["target"].toString();
        QString senderName = obj["sender"].toString();

        if (clientList.contains(target)) {
            clientList[target]->write(data); // 一對一轉發 JSON 封包
            logEdit->append(QString("訊息轉發: %1 -> %2").arg(senderName).arg(target));
        }
    }
}

// 斷線
void Widget::onDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        QString nickname = clientList.key(socket);
        if (!nickname.isEmpty()) {
            clientList.remove(nickname);
            logEdit->append(QString("使用者離開: %1").arg(nickname));
            updateUserList(); // 通知其他人
        }
        socket->deleteLater();
    }
}

//好友名單
void Widget::updateUserList() {
    QJsonObject res;
    res["type"] = "userlist";
    QJsonArray users;
    for (const QString &name : clientList.keys()) {
        users.append(name);
    }
    res["users"] = users;

    QByteArray data = QJsonDocument(res).toJson();
    for (QTcpSocket *s : clientList.values()) {
        s->write(data);
    }
}
