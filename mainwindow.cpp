#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QHostAddress>
#include <QFile>
#include <QFileInfo>
#include <QApplication>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpServer(new QTcpServer(this))
    ,clientSocket(new QTcpSocket(this))
    ,serverClientSocket(nullptr)
{
    ui->setupUi(this);
    initDatabase();
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

//服务端功能↓
void MainWindow::onNewConnection()
{
    serverClientSocket = tcpServer->nextPendingConnection();
    ui->serverLogTextEdit->append("有新的客户端连接进来");

    // 重置接收状态
    recvBuffer.clear();
    waitingForFile = false;

    connect(serverClientSocket, &QTcpSocket::readyRead, this, [=]() {
        recvBuffer.append(serverClientSocket->readAll());
        processBuffer();//开始循环解析
    });

    connect(serverClientSocket, &QTcpSocket::disconnected, this, [=]() {
        ui->serverLogTextEdit->append("客户端已断开连接");
        serverClientSocket->deleteLater();
    });
}

// ===== 循环解析缓冲区 =====
void MainWindow::processBuffer()
{
    // 修复收发文件卡死
    while (!recvBuffer.isEmpty()) {
        if (waitingForFile) {
            if (!handleChunk()) break;            // 数据不够
        } else if (recvBuffer.startsWith("FILE:")) {
            if (!handleFileHeader()) break;       // 头部没收完
        } else {
            handleTextMessage();
        }
    }
}

// ===== 解析 FILE: 头部 =====
bool MainWindow::handleFileHeader()
{
    int newlinePos = recvBuffer.indexOf('\n');
    if (newlinePos == -1) return false;  // 头部还没收完

    // 提取 "文件名:大小:总片数"
    QByteArray headerBytes = recvBuffer.mid(5, newlinePos - 5);
    QStringList parts = QString::fromUtf8(headerBytes).split(":");
    recvFileName = parts[0];
    recvFileSize = parts[1].toLongLong();
    recvTotalChunks = parts[2].toInt();

    recvBuffer.remove(0, newlinePos + 1);  // 移除头部
    waitingForFile = true;
    recvReceivedChunks = 0;
    recvFileBuffer.clear();

    ui->receiveProgressBar->setMaximum(recvTotalChunks);
    ui->receiveProgressBar->setValue(0);
    ui->serverLogTextEdit->append(
        QString("正在接收文件：%1 (%2 字节, %3 片)")
            .arg(recvFileName).arg(recvFileSize).arg(recvTotalChunks));
     return true;
}

// ===== 解析 CHUNK: 分片 =====
bool MainWindow::handleChunk()
{
    if (!recvBuffer.startsWith("CHUNK:")) return false;

    int newlinePos = recvBuffer.indexOf('\n');
    if (newlinePos == -1) return false;  // 头部还没收完

    // 计算这一片数据大小：标准 64KB，最后一片可能更小
    qint64 chunkSize = qMin(qint64(64 * 1024), recvFileSize - recvFileBuffer.size());
    int dataStart = newlinePos + 1;
    if (recvBuffer.size() - dataStart < chunkSize) return false;  // 数据不够，等下次

    // 取出分片数据，拼到文件缓冲区
    recvFileBuffer.append(recvBuffer.mid(dataStart, chunkSize));
    recvBuffer.remove(0, dataStart + chunkSize);

    recvReceivedChunks++;
    ui->receiveProgressBar->setValue(recvReceivedChunks);

    if (recvReceivedChunks >= recvTotalChunks) {
        QFile file(recvFileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(recvFileBuffer);
            file.close();
        }
        ui->serverLogTextEdit->append(
            QString("文件接收完成：%1 (%2 字节)").arg(recvFileName).arg(recvFileSize));
        logFile("client", recvFileName, recvFileSize);
        waitingForFile = false;
        recvFileBuffer.clear();
    }
     return true;
}

// ===== 解析文本消息 =====
void MainWindow::handleTextMessage()
{
    int newlinePos = recvBuffer.indexOf('\n');
    QByteArray line = (newlinePos == -1) ? recvBuffer : recvBuffer.left(newlinePos);
    recvBuffer.remove(0, (newlinePos == -1) ? recvBuffer.size() : newlinePos + 1);

    QString text = QString::fromUtf8(line);
    if (!text.isEmpty()) {
        ui->serverLogTextEdit->append("收到消息：" + text);
        logMessage("client", text);
    }
}
//服务端功能↑

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
    logMessage("client", message);
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

    const qint64 chunkSize = 64 * 1024;                         //规定每片64kb
    int totalChunks = (fileSize + chunkSize - 1)/ chunkSize;    //切片数量 向上取整


    //1.发送文件头部：FILE:文件名:大小:总片数
    QString header = QString("FILE:%1:%2:%3\n").arg(fileName).arg(fileSize).arg(totalChunks);
    clientSocket->write(header.toUtf8());

    //2.逐片发送
    ui->sendProgressBar->setMaximum(totalChunks);
    ui->sendProgressBar->setValue(0);

    for(int i = 0; i < totalChunks;i++){
        //切出当前切片
        QByteArray chunk = fileData.mid(i*chunkSize,chunkSize);

        //发送分片头部
        QString chunkHeader = QString("CHUNK:%1\n").arg(i);
        clientSocket->write(chunkHeader.toUtf8());

        //发送分片数据
        clientSocket->write(chunk);

        //更新进度条
        ui->sendProgressBar->setValue(i+1);

        //处理积压事务 防止程序卡死
        QApplication::processEvents();
    }

    ui->clientLogTextEdit->append(QString("已发送文件%1（%2字节，%3片）").arg(fileName).arg(fileSize).arg(totalChunks));

    //数据库接受
    logFile("client",fileName,fileSize);

}


void MainWindow::initDatabase()
{
    //创建sqlite数据库连接 并保存在程序目录下的transfer.db
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("transfer.db");

    if(!db.open()){
        ui->serverLogTextEdit->append("数据库打开失败：" + db.lastError().text());
        return;
    }

    //创建传输记录表（首次）
    QSqlQuery query;
    query.exec(
        "CREATE TABLE IF NOT EXISTS transfer_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "type TEXT NOT NULL,"          // 'message' 或 'file'
        "sender TEXT NOT NULL,"        // 'client' 或 'server'
        "content TEXT,"                // 文本内容或文件名
        "file_size INTEGER,"           // 文件大小，文本消息为 NULL
        "time DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")"
        );
    ui->serverLogTextEdit->append("数据库初始化完成");
}

void MainWindow::logMessage(const QString &sender,const QString &content)
{
    QSqlQuery query;
    query.prepare("INSERT INTO transfer_log (type,sender,content) VALUES ('message',?,?)");
    query.addBindValue(sender);
    query.addBindValue(content);
    query.exec();
}

void MainWindow::logFile(const QString &sender,const QString &fileName,qint64 fileSize)
{
    QSqlQuery query;
    query.prepare("INSERT INTO transfer_log (type, sender, content, file_size) VALUES ('file', ?, ?, ?)");
                                    //信息插入日志 分别为类型 发送者 内容 文件大小 初始为 类型：文件
    query.addBindValue(sender);     //添加发送者
    query.addBindValue(fileName);   //..
    query.addBindValue(fileSize);   //..
    query.exec();                   //执行sql
}









