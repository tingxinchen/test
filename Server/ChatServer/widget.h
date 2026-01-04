#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QMap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    // Server
    void startServer();       // 啟動
    void stopServer();        // 關閉伺服器

    // 事件
    void onNewConnection();   // 處理Client連入
    void onReadyRead();       // 轉發訊息
    void onDisconnected();    // 處理成員離開並更新名單

private:
    // UI
    QLineEdit *ipEdit, *portEdit;
    QPushButton *startBtn, *stopBtn;
    QTextEdit *logEdit;       // 伺服器日誌

    // 核心
    QTcpServer *tcpServer;
    QMap<QString, QTcpSocket*> clientList; // 在線名單[暱稱 -> 使用者]

    // 輔助
    void updateUserList();    // 廣播最新在線名單
    void setupUI();           // 介面
};

#endif
