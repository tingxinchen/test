#include "chatclient.h"
#include "ui_chatclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDateTime>

ChatClient::ChatClient(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChatClient)
    , socket(new QTcpSocket(this))
{
    ui->setupUi(this);

    // 1. 接收訊息
    connect(socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);

    // 2. 連線成功：切換按鈕狀態
    connect(socket, &QTcpSocket::connected, this, [=](){
        ui->chatDisplay->addItem("系統提示: 成功連線到伺服器！");
        ui->connectBtn->setEnabled(false);
        ui->disconnectBtn->setEnabled(true);
    });

    // 3. 偵測斷開：清空資料並恢復介面
    connect(socket, &QTcpSocket::disconnected, this, [=](){
        ui->chatDisplay->addItem("系統提示: 連線已斷開。");
        ui->connectBtn->setEnabled(true);
        ui->disconnectBtn->setEnabled(false);
        ui->userList->clear();
        currentTarget = "";
    });

    // 4. 點擊好友名單
    connect(ui->userList, &QListWidget::itemClicked, this, &ChatClient::onUserSelected);
}

ChatClient::~ChatClient() {
    socket->close();
    delete ui;
}

// --- 按鈕功能：連線 ---
void ChatClient::on_connectBtn_clicked() {
    myName = ui->nameEdit->text().trimmed();
    if (myName.isEmpty()) {
        myName = "匿名用戶";
        ui->nameEdit->setText(myName);
    }

    socket->connectToHost(ui->ipEdit->text(), ui->Port->text().toUShort());

    if (socket->waitForConnected(3000)) {
        QJsonObject login;
        login["type"] = "login";
        login["nickname"] = myName;
        socket->write(QJsonDocument(login).toJson());
    } else {
        QMessageBox::critical(this, "錯誤", "無法連線至伺服器");
    }
}

// --- 按鈕功能：中斷 ---
void ChatClient::on_disconnectBtn_clicked() {
    socket->disconnectFromHost();
}

// --- 處理名單選取 ---
void ChatClient::onUserSelected() {
    if (ui->userList->currentItem()) {
        currentTarget = ui->userList->currentItem()->text();
        if (currentTarget == "群體聊天") {
            ui->chatDisplay->addItem(">>> 已進入群聊模式");
        } else {
            ui->chatDisplay->addItem(QString(">>> 與 %1 私訊中").arg(currentTarget));
        }
    }
}

// --- 傳送文字 ---
void ChatClient::on_sendBtn_clicked() {
    if (currentTarget.isEmpty()) {
        QMessageBox::warning(this, "提示", "請先選擇聊天對象");
        return;
    }

    QString msg = ui->msgEdit->text();
    if (msg.isEmpty()) return;

    QJsonObject chat;
    chat["type"] = "chat";
    chat["sender"] = myName;
    chat["target"] = currentTarget; // 可能是具體名字，也可能是 "群體聊天"
    chat["content"] = msg;

    socket->write(QJsonDocument(chat).toJson());

    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->chatDisplay->addItem(QString("[%1] 我: %2").arg(time).arg(msg));
    ui->msgEdit->clear();
}

// --- 傳送檔案 ---
void ChatClient::on_fileBtn_clicked() {
    if (currentTarget.isEmpty()) return;

    QString path = QFileDialog::getOpenFileName(this, "選擇檔案");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonObject fileObj;
        fileObj["type"] = "chat";
        fileObj["sender"] = myName;
        fileObj["target"] = currentTarget;
        fileObj["fileName"] = QFileInfo(path).fileName();
        fileObj["fileContent"] = QString(file.readAll().toBase64());

        socket->write(QJsonDocument(fileObj).toJson());
        ui->chatDisplay->addItem(QString("已傳送檔案: %1").arg(fileObj["fileName"].toString()));
    }
}

// --- 核心：接收處理中心 ---
void ChatClient::onReadyRead() {
    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    // 更新名單：固定加入群聊選項
    if (type == "userlist") {
        ui->userList->clear();
        ui->userList->addItem("群體聊天");
        QJsonArray users = obj["users"].toArray();
        for (int i = 0; i < users.size(); ++i) {
            QString name = users[i].toString();
            if (name != myName) ui->userList->addItem(name);
        }
    }
    // 處理聊天訊息 (文字或檔案)
    else if (type == "chat") {
        QString sender = obj["sender"].toString();
        QString target = obj["target"].toString();
        QString time = QDateTime::currentDateTime().toString("hh:mm:ss");

        // 判斷是否為群聊訊息（非發送給自己的私訊且標記為群聊）
        QString displayTag = (target == "群體聊天") ? "[群聊]" : "";

        if (obj.contains("fileContent")) {
            if (QMessageBox::question(this, "收到檔案", QString("來自 %1 的檔案，是否儲存？").arg(sender)) == QMessageBox::Yes) {
                QString path = QFileDialog::getSaveFileName(this, "儲存", obj["fileName"].toString());
                if (!path.isEmpty()) {
                    QFile f(path);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(QByteArray::fromBase64(obj["fileContent"].toString().toUtf8()));
                        f.close();
                    }
                }
            }
        } else {
            ui->chatDisplay->addItem(QString("[%1] %2%3: %4").arg(time).arg(displayTag).arg(sender).arg(obj["content"].toString()));
        }
    }
}
