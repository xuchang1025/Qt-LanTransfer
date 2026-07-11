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

void MainWindow::onNewConnection()
{
    serverClientSocket = tcpServer->nextPendingConnection();
    ui->serverLogTextEdit->append("有新的客户端连接进来");

    // ---- 文件接收相关 ----
    QString fileName;
    qint64 fileSize = 0;
    int totalChunks = 0;
    int receivedChunks = 0;
    QByteArray fileBuffer;

    // ---- 数据缓冲区 ----
    QByteArray buffer;       // 累积所有收到的原始数据

    // ---- 状态 ----
    bool waitingForFile = false;  // true = 正在接收文件

    connect(serverClientSocket, &QTcpSocket::readyRead, this, [=]() mutable {
        // ① 把新数据追加到缓冲区
        buffer.append(serverClientSocket->readAll());

        // ② 循环解析缓冲区
        while (!buffer.isEmpty()) {

            if (waitingForFile) {
                // ===== 文件模式：找 CHUNK: =====
                if (!buffer.startsWith("CHUNK:")) {
                    break;  // 数据不完整，等下次
                }

                int newlinePos = buffer.indexOf('\n');
                if (newlinePos == -1) break;  // \n 还没到，等下次

                // 解析分片序号
                int chunkIndex = buffer.mid(6, newlinePos - 6).toInt();

                // 计算这一片数据有多少字节
                // 除了最后一片，其他都是 64KB
                qint64 chunkDataSize = qMin(qint64(64 * 1024), fileSize - fileBuffer.size());

                // 检查缓冲区里的数据够不够
                int dataStart = newlinePos + 1;
                if (buffer.size() - dataStart < chunkDataSize) {
                    break;  // 数据还没到齐，等下次
                }

                // 取出这一片的数据，追加到文件缓冲区
                fileBuffer.append(buffer.mid(dataStart, chunkDataSize));

                // 从 buffer 中移除已处理的部分
                buffer.remove(0, dataStart + chunkDataSize);

                receivedChunks++;
                ui->receiveProgressBar->setValue(receivedChunks);

                if (receivedChunks >= totalChunks) {
                    // 接收完毕
                    QFile file(fileName);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(fileBuffer);
                        file.close();
                        ui->serverLogTextEdit->append(
                            QString("文件接收完成：%1 (%2 字节)").arg(fileName).arg(fileSize));
                    }
                    logFile("client", fileName, fileSize);

                    waitingForFile = false;
                    fileBuffer.clear();
                }

            } else {
                // ===== 普通模式：找 FILE: 或文本 =====
                if (buffer.startsWith("FILE:")) {
                    int newlinePos = buffer.indexOf('\n');
                    if (newlinePos == -1) break;

                    // 解析头部
                    QByteArray headerBytes = buffer.mid(5, newlinePos - 5);
                    QString header = QString::fromUtf8(headerBytes);
                    QStringList parts = header.split(":");
                    fileName = parts[0];
                    fileSize = parts[1].toLongLong();
                    totalChunks = parts[2].toInt();

                    buffer.remove(0, newlinePos + 1);  // 移除头部

                    waitingForFile = true;
                    receivedChunks = 0;
                    fileBuffer.clear();

                    ui->receiveProgressBar->setMaximum(totalChunks);
                    ui->receiveProgressBar->setValue(0);

                    ui->serverLogTextEdit->append(
                        QString("正在接收文件：%1 (%2 字节, %3 片)")
                            .arg(fileName).arg(fileSize).arg(totalChunks));

                } else {
                    // 普通文本：取到 \n 为止，或者全部取走
                    int newlinePos = buffer.indexOf('\n');
                    if (newlinePos == -1) {
                        // 没有 \n，全部当一条消息
                        QString text = QString::fromUtf8(buffer);
                        ui->serverLogTextEdit->append("收到消息：" + text);
                        logMessage("client", text);
                        buffer.clear();
                    } else {
                        QByteArray line = buffer.left(newlinePos);
                        buffer.remove(0, newlinePos + 1);
                        QString text = QString::fromUtf8(line);
                        if (!text.isEmpty()) {
                            ui->serverLogTextEdit->append("收到消息：" + text);
                            logMessage("client", text);
                        }
                    }
                }
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









