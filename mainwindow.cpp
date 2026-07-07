#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QHostAddress>
#include <QFile>
#include <QFileInfo>

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

    //双端通信
    connect(ui->replyButton,&QPushButton::clicked,
            this,&MainWindow::onReplyClicked);

    connect(clientSocket,&QTcpSocket::readyRead,
            this,[=](){
        QByteArray data = clientSocket->readAll();

        if (data.isEmpty()) return;
        QString message = QString::fromUtf8(data);
        ui->clientLogTextEdit->append("服务端回复" + message);
    });

    //文件传输
    connect(ui->selectFileButton,&QPushButton::clicked,
            this,&MainWindow::onSelectFileClicked);

    connect(ui->sendFileButton,&QPushButton::clicked,
            this,&MainWindow::onSendFileClicked);
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

    bool receivingFile = false;    // 判断是否正在接收文件
    QString fileName;              // 当前接收的文件名
    qint64 fileSize = 0;           // 当前接收的文件大小
    QByteArray fileBuffer;         // 文件数据缓冲区

    connect(serverClientSocket, &QTcpSocket::readyRead, this, [=]() mutable {
        QByteArray data = serverClientSocket->readAll();
        if (data.isEmpty()) return;

        if (receivingFile) {
            // 状态：正在接收文件数据
            fileBuffer.append(data);

            if (fileBuffer.size() >= fileSize) {
                // 文件接收完毕，保存到磁盘
                QFile file(fileName);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(fileBuffer);
                    file.close();
                    ui->serverLogTextEdit->append(
                        QString("文件接收完成：%1 (%2 字节)").arg(fileName).arg(fileSize));
                }
                // 接收完成，重置接收判断以及文件缓冲区
                receivingFile = false;
                fileBuffer.clear();
            }
        } else {
            // 状态：先看是不是文件头部
            QString text = QString::fromUtf8(data);
            if (text.startsWith("FILE:")) {
                // 解析文件头部：FILE:文件名:大小
                QStringList parts = text.mid(5).split(":");
                fileName = parts[0];                    // 文件名
                fileSize = parts[1].toLongLong();       // 文件大小
                receivingFile = true;
                fileBuffer.clear();

                ui->serverLogTextEdit->append(
                    QString("正在接收文件：%1 (%2 字节)").arg(fileName).arg(fileSize));

                // 头部后面的数据（如果有）就是文件内容，存入缓冲区
                int headerEnd = text.indexOf('\n') + 1;
                if (headerEnd < data.size()) {
                    fileBuffer.append(data.mid(headerEnd));
                }
            } else {
                // 普通文本消息
                ui->serverLogTextEdit->append("收到消息：" + text);
            }
        }
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

//文件传输
void MainWindow::onSelectFileClicked()
{
    selectedFilePath = QFileDialog::getOpenFileName(this, "选择文件");
    if (selectedFilePath.isEmpty()) return;

    ui->filePathLabel->setText(selectedFilePath);  // 显示路径
}

void MainWindow::onSendFileClicked()
{
    if (selectedFilePath.isEmpty()) {
        ui->clientLogTextEdit->append("请先选择文件");
        return;
    }

    QFile file(selectedFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ui->clientLogTextEdit->append("无法打开文件");
        return;
    }

    QByteArray fileData = file.readAll();        // 读取全部文件内容
    file.close();

    QString fileName = QFileInfo(selectedFilePath).fileName();  // 文件名（不含路径）
    qint64 fileSize = fileData.size();                          // 文件大小

    // 发送文件头部：FILE:文件名:大小\n
    QString header = QString("FILE:%1:%2\n").arg(fileName).arg(fileSize);
    clientSocket->write(header.toUtf8());

    // 发送文件数据
    clientSocket->write(fileData);

    ui->clientLogTextEdit->append(
        QString("已发送文件：%1 (%2 字节)").arg(fileName).arg(fileSize));
}