#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

#include <QHostAddress>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpServer(new QTcpServer(this))
    ,clientSocket(new QTcpSocket(this))
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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onStartListenClicked()
{
    quint16 port = ui->portLineEdit->text().toUShort();

    bool success = tcpServer->listen(QHostAddress::Any, port);

    if (success) {
        ui->serverLogTextEdit->append(QString("服务端已启动，正在监听端口：%1").arg(port));
        ui->startListenButton->setEnabled(false);
    } else {
        ui->serverLogTextEdit->append("服务端启动失败：" + tcpServer->errorString());
    }
}

void MainWindow::onNewConnection()
{
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();

    ui->serverLogTextEdit->append("有新的客户端连接进来");

    connect(clientSocket, &QTcpSocket::readyRead, this, [=]() {
        QByteArray data = clientSocket->readAll();
        QString message = QString::fromUtf8(data);

        ui->serverLogTextEdit->append("收到消息：" + message);
    });

    connect(clientSocket, &QTcpSocket::disconnected, this, [=]() {
        ui->serverLogTextEdit->append("客户端已断开连接");
        clientSocket->deleteLater();
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