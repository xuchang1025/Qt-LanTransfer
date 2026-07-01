#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

#include <QHostAddress>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpServer(new QTcpServer(this))
    ,clientSocket(new QTcpSocket(this))
    ,serverClientSocket(nullptr)
{
    ui->setupUi(this);

    ui->portLineEdit->setText("8888");
    ui->serverLogTextEdit->setReadOnly(true);

    connect(ui->startListenButton, &QPushButton::clicked,//信号是开始监听按钮被点击 运行槽函数
            this, &MainWindow::onStartListenClicked);

    connect(tcpServer, &QTcpServer::newConnection,//信号是TCP server有新的连接信号 运行槽函数
            this, &MainWindow::onNewConnection);

    //客户端绑定
    connect(ui->connectButton,&QPushButton::clicked,
            this,&MainWindow::onConnectClicked);

    connect(ui->sendButton,&QPushButton::clicked,
            this,&MainWindow::onSendClicked);

    connect(clientSocket,&QTcpSocket::connected,this,[=](){
        ui->clientLogTextEdit->append("连接成功");
    });

    connect(clientSocket,&QTcpSocket::errorOccurred,this,[=](){
        ui->clientLogTextEdit->append("连接失败：" + clientSocket->errorString());
    });

    connect(ui->replyButton,&QPushButton::clicked,
            this,&MainWindow::onReplyClicked);

    connect(clientSocket,&QTcpSocket::readyRead,this,[=](){
        QByteArray data = clientSocket->readAll();

        if (data.isEmpty()) return;
        QString message = QString::fromUtf8(data);
        ui->clientLogTextEdit->append("服务端回复" + message);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onStartListenClicked()
{
    // 新增切换端口监听（监听中）
    if (tcpServer->isListening()) {
        tcpServer->close();                                          // 关闭服务端
        ui->serverLogTextEdit->append("服务端已停止监听");
        ui->startListenButton->setText("启动监听");                   // 按钮文字改回来
        ui->portLineEdit->setEnabled(true);                          // 允许修改端口
        return;
    }

    // 启动监听
    quint16 port = ui->portLineEdit->text().toUShort();
    bool success = tcpServer->listen(QHostAddress::Any, port);

    if (success) {
        ui->serverLogTextEdit->append(
            QString("服务端已启动，正在监听端口：%1").arg(port));
        ui->startListenButton->setText("停止监听");                   // 按钮文字改成"停止"
        ui->portLineEdit->setEnabled(false);                         // 禁止修改端口
    } else {
        ui->serverLogTextEdit->append("服务端启动失败：" + tcpServer->errorString());
    }
}

void MainWindow::onNewConnection()
{
    serverClientSocket = tcpServer->nextPendingConnection();

    ui->serverLogTextEdit->append("有新的客户端连接进来");

    connect(serverClientSocket, &QTcpSocket::readyRead, this, [=]() {
        QByteArray data = serverClientSocket->readAll();
        QString message = QString::fromUtf8(data);

        if (data.isEmpty()) return;
        ui->serverLogTextEdit->append("收到消息：" + message);
    });

    connect(serverClientSocket, &QTcpSocket::disconnected, this, [=]() {
        ui->serverLogTextEdit->append("客户端已断开连接");
        serverClientSocket->deleteLater();
    });
}

void MainWindow::onConnectClicked()
{
    qDebug()<<"clientsocket 指针"<<clientSocket;
    QString ip = ui->serverIpLineEdit->text();
    quint16 port = ui->ServerPortLineEdit->text().toUShort();

    ui->clientLogTextEdit->append(
        QString("正在连接 %1:%2...").arg(ip).arg(port));

    clientSocket->connectToHost(ip,port);

}

void MainWindow::onSendClicked()
{
    QString message = ui->messageLineEdit->text();
    if(message.isEmpty())return;

    clientSocket->write(message.toUtf8());

    ui->clientLogTextEdit->append("已发送：" + message);
    ui->messageLineEdit->clear();
}

void MainWindow::onReplyClicked()
{
    QString message = ui->replyLineEdit->text();
    if(message.isEmpty())return;

    serverClientSocket->write(message.toUtf8());

    ui->serverLogTextEdit->append("已回复：" + message);
    ui->replyLineEdit->clear();
}