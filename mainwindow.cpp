#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "filesender.h"
#include <QDebug>
#include <QHostAddress>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpServer(new QTcpServer(this))
    , clientSocket(new QTcpSocket(this))
    , serverClientSocket(nullptr)
    , logModel(nullptr)
    , logTableView(nullptr)
    , fileSender(nullptr)
{
    ui->setupUi(this);
    initDatabase();
    setupTransferHistory();
    setupStyle();

    ui->portLineEdit->setText("8888");
    ui->serverLogTextEdit->setReadOnly(true);

    connect(ui->startListenButton, &QPushButton::clicked,
            this, &MainWindow::onStartListenClicked);

    connect(tcpServer, &QTcpServer::newConnection,
            this, &MainWindow::onNewConnection);

    connect(ui->connectButton, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);

    connect(ui->sendButton, &QPushButton::clicked,
            this, &MainWindow::onSendClicked);

    connect(clientSocket, &QTcpSocket::connected, this, [=]() {
        ui->clientLogTextEdit->append("连接成功");
    });

    connect(clientSocket, &QTcpSocket::errorOccurred, this, [=]() {
        ui->clientLogTextEdit->append("连接失败：" + clientSocket->errorString());
    });

    connect(ui->replyButton, &QPushButton::clicked,
            this, &MainWindow::onReplyClicked);

    connect(clientSocket, &QTcpSocket::readyRead,
            this, [=]() {
        QByteArray data = clientSocket->readAll();
        if (data.isEmpty()) return;
        QString message = QString::fromUtf8(data);
        ui->clientLogTextEdit->append("服务端回复" + message);
    });

    connect(ui->selectFileButton, &QPushButton::clicked,
            this, &MainWindow::onSelectFileClicked);

    connect(ui->sendFileButton, &QPushButton::clicked,
            this, &MainWindow::onSendFileClicked);

    connect(ui->clearHistoryButton, &QPushButton::clicked,
            this, &MainWindow::onClearHistoryClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ===== 传输历史表格（Model/View 架构） =====
void MainWindow::setupTransferHistory()
{
    // QSqlTableModel 自动映射数据库表到 Model
    logModel = new QSqlTableModel(this);
    logModel->setTable("transfer_log");
    logModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    logModel->select();  // 执行 SELECT * FROM transfer_log

    // 设置列标题（中文）
    logModel->setHeaderData(0, Qt::Horizontal, "ID");
    logModel->setHeaderData(1, Qt::Horizontal, "类型");
    logModel->setHeaderData(2, Qt::Horizontal, "发送方");
    logModel->setHeaderData(3, Qt::Horizontal, "内容");
    logModel->setHeaderData(4, Qt::Horizontal, "文件大小");
    logModel->setHeaderData(5, Qt::Horizontal, "时间");

    // 创建 QTableView 并绑定 Model
    logTableView = new QTableView(this);
    logTableView->setModel(logModel);
    logTableView->setGeometry(10, 360, 960, 280);  // 放在窗口底部
    logTableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 整行选中
    logTableView->setAlternatingRowColors(true);    // 交替行颜色
    logTableView->horizontalHeader()->setStretchLastSection(true);  // 最后一列自适应
    logTableView->setSortingEnabled(true);          // 点击表头排序

    // 隐藏 ID 列（用户不需要看）
    logTableView->setColumnHidden(0, true);

    // 设置列宽
    logTableView->setColumnWidth(1, 60);
    logTableView->setColumnWidth(2, 60);
    logTableView->setColumnWidth(3, 200);
    logTableView->setColumnWidth(4, 80);
}

// 刷新表格（每次记录新消息/文件后调用）
void MainWindow::refreshLogTable()
{
    if (logModel) {
        logModel->select();  // 重新查询最新数据
    }
}

// ===== QSS 界面美化 =====
void MainWindow::setupStyle()
{
    this->setStyleSheet(R"(
        /* ==== 全局 ==== */
        QMainWindow {
            background-color: #f0f2f5;
        }
        QWidget#centralwidget {
            background-color: #f0f2f5;
        }

        /* ==== 按钮 ==== */
        QPushButton {
            background-color: #4a90d9;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 5px 12px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #357abd;
        }
        QPushButton:pressed {
            background-color: #2a5f9e;
        }

        /* ==== 输入框 ==== */
        QLineEdit {
            border: 1px solid #d0d5dd;
            border-radius: 4px;
            padding: 4px 8px;
            background-color: white;
            font-size: 13px;
        }
        QLineEdit:focus {
            border-color: #4a90d9;
        }

        /* ==== 日志框 ==== */
        QTextEdit {
            border: 1px solid #d0d5dd;
            border-radius: 4px;
            background-color: white;
            font-size: 13px;
            padding: 4px;
        }

        /* ==== 进度条 ==== */
        QProgressBar {
            border: 1px solid #d0d5dd;
            border-radius: 4px;
            text-align: center;
            background-color: #e8ecf1;
            font-size: 11px;
        }
        QProgressBar::chunk {
            background-color: #4a90d9;
            border-radius: 3px;
        }

        /* ==== 表格 ==== */
        QTableView {
            border: 1px solid #d0d5dd;
            border-radius: 4px;
            background-color: white;
            gridline-color: #e8ecf1;
            font-size: 13px;
        }
        QTableView::item {
            padding: 4px 8px;
        }
        QTableView::item:selected {
            background-color: #4a90d9;
            color: white;
        }
        QHeaderView::section {
            background-color: #e8ecf1;
            padding: 6px 8px;
            border: none;
            border-bottom: 1px solid #d0d5dd;
            font-weight: bold;
            font-size: 13px;
        }

        /* ==== 标签 ==== */
        QLabel {
            font-size: 13px;
            color: #333;
        }
    )");
}

// ===== 服务端功能 =====
void MainWindow::onStartListenClicked()
{
    if (tcpServer->isListening()) {
        tcpServer->close();
        ui->serverLogTextEdit->append("服务端已停止监听");
        ui->startListenButton->setText("启动监听");
        ui->portLineEdit->setEnabled(true);
        return;
    }

    quint16 port = ui->portLineEdit->text().toUShort();
    bool success = tcpServer->listen(QHostAddress::Any, port);

    if (success) {
        ui->serverLogTextEdit->append(
            QString("服务端已启动，正在监听端口：%1").arg(port));
        ui->startListenButton->setText("停止监听");
        ui->portLineEdit->setEnabled(false);
    } else {
        ui->serverLogTextEdit->append("服务端启动失败：" + tcpServer->errorString());
    }
}

void MainWindow::onNewConnection()
{
    serverClientSocket = tcpServer->nextPendingConnection();
    ui->serverLogTextEdit->append("有新的客户端连接进来");

    recvBuffer.clear();
    waitingForFile = false;

    connect(serverClientSocket, &QTcpSocket::readyRead, this, [=]() {
        recvBuffer.append(serverClientSocket->readAll());
        processBuffer();
    });

    connect(serverClientSocket, &QTcpSocket::disconnected, this, [=]() {
        ui->serverLogTextEdit->append("客户端已断开连接");
        serverClientSocket->deleteLater();
    });
}

void MainWindow::processBuffer()
{
    while (!recvBuffer.isEmpty()) {
        if (waitingForFile) {
            if (!handleChunk()) break;
        } else if (recvBuffer.startsWith("FILE:")) {
            if (!handleFileHeader()) break;
        } else {
            handleTextMessage();
        }
    }
}

bool MainWindow::handleFileHeader()
{
    int newlinePos = recvBuffer.indexOf('\n');
    if (newlinePos == -1) return false;

    QByteArray headerBytes = recvBuffer.mid(5, newlinePos - 5);
    QStringList parts = QString::fromUtf8(headerBytes).split(":");
    recvFileName = parts[0];
    recvFileSize = parts[1].toLongLong();
    recvTotalChunks = parts[2].toInt();

    recvBuffer.remove(0, newlinePos + 1);
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

bool MainWindow::handleChunk()
{
    if (!recvBuffer.startsWith("CHUNK:")) return false;

    int newlinePos = recvBuffer.indexOf('\n');
    if (newlinePos == -1) return false;

    qint64 chunkSize = qMin(qint64(64 * 1024), recvFileSize - recvFileBuffer.size());
    int dataStart = newlinePos + 1;
    if (recvBuffer.size() - dataStart < chunkSize) return false;

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
        refreshLogTable();  // 刷新传输记录表格
        waitingForFile = false;
        recvFileBuffer.clear();
    }
    return true;
}

void MainWindow::handleTextMessage()
{
    int newlinePos = recvBuffer.indexOf('\n');
    QByteArray line = (newlinePos == -1) ? recvBuffer : recvBuffer.left(newlinePos);
    recvBuffer.remove(0, (newlinePos == -1) ? recvBuffer.size() : newlinePos + 1);

    QString text = QString::fromUtf8(line);
    if (!text.isEmpty()) {
        ui->serverLogTextEdit->append("收到消息：" + text);
        logMessage("client", text);
        refreshLogTable();  // 刷新传输记录表格
    }
}

// ===== 客户端功能 =====
void MainWindow::onConnectClicked()
{
    qDebug() << "clientsocket 指针" << clientSocket;
    QString ip = ui->serverIpLineEdit->text();
    quint16 port = ui->ServerPortLineEdit->text().toUShort();

    ui->clientLogTextEdit->append(
        QString("正在连接 %1:%2...").arg(ip).arg(port));

    clientSocket->connectToHost(ip, port);
}

void MainWindow::onSendClicked()
{
    QString message = ui->messageLineEdit->text();
    if (message.isEmpty()) return;

    clientSocket->write(message.toUtf8());

    ui->clientLogTextEdit->append("已发送：" + message);
    ui->messageLineEdit->clear();
}

void MainWindow::onReplyClicked()
{
    QString message = ui->replyLineEdit->text();
    if (message.isEmpty()) return;

    serverClientSocket->write(message.toUtf8());
    logMessage("server", message);
    refreshLogTable();  // 刷新传输记录表格
    ui->serverLogTextEdit->append("已回复：" + message);
    ui->replyLineEdit->clear();
}

// ===== 文件传输 =====
void MainWindow::onSelectFileClicked()
{
    selectedFilePath = QFileDialog::getOpenFileName(this, "选择文件");
    if (selectedFilePath.isEmpty()) return;

    ui->filePathLabel->setText(selectedFilePath);
}

void MainWindow::onSendFileClicked()
{
    if (selectedFilePath.isEmpty()) {
        ui->clientLogTextEdit->append("请先选择文件");
        return;
    }

    // 禁用按钮，防止重复点击
    ui->sendFileButton->setEnabled(false);
    ui->clientLogTextEdit->append("正在准备发送文件...");

    // 创建 FileSender 工作对象（自动在子线程中运行）
    fileSender = new FileSender(this);

    // ---- 连接信号：头部信息 → 主线程写 socket ----
    connect(fileSender, &FileSender::headerReady, this,
            [this](const QString &fileName, qint64 fileSize, int totalChunks) {
        QString header = QString("FILE:%1:%2:%3\n")
                             .arg(fileName).arg(fileSize).arg(totalChunks);
        clientSocket->write(header.toUtf8());
        ui->sendProgressBar->setMaximum(totalChunks);
        ui->sendProgressBar->setValue(0);
    });

    // ---- 连接信号：每片数据 → 主线程写 socket ----
    connect(fileSender, &FileSender::chunkReady, this,
            [this](int index, const QByteArray &chunkData) {
        QString chunkHeader = QString("CHUNK:%1\n").arg(index);
        clientSocket->write(chunkHeader.toUtf8());
        clientSocket->write(chunkData);
    });

    // ---- 连接信号：进度更新 → 更新进度条 ----
    connect(fileSender, &FileSender::progressUpdated, this,
            [this](int current, int /*total*/) {
        ui->sendProgressBar->setValue(current);
    });

    // ---- 连接信号：发送完成 → 清理 ----
    connect(fileSender, &FileSender::finished, this,
            [this](const QString &fileName, qint64 fileSize) {
        ui->clientLogTextEdit->append(
            QString("已发送文件 %1").arg(fileName));
        logFile("client", fileName, fileSize);
        refreshLogTable();  // 刷新传输记录表格
        ui->sendFileButton->setEnabled(true);
        fileSender = nullptr;  // FileSender 会自动 deleteLater
    });

    // ---- 连接信号：发送出错 → 提示 ----
    connect(fileSender, &FileSender::errorOccurred, this,
            [this](const QString &error) {
        ui->clientLogTextEdit->append("发送失败：" + error);
        ui->sendFileButton->setEnabled(true);
        fileSender = nullptr;
    });

    // 启动发送（子线程中执行文件读取）
    fileSender->start(selectedFilePath);
}

// ===== 清除传输记录 =====
void MainWindow::onClearHistoryClicked()
{
    // 弹出确认对话框，防止误删
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认清除", "确定要清除所有传输记录和接收的文件吗？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // 1. 先查询要删除的文件名（在清空表之前）
    QStringList fileNames;
    QSqlQuery fileQuery;
    fileQuery.exec("SELECT content FROM transfer_log WHERE type='file'");
    while (fileQuery.next()) {
        fileNames.append(fileQuery.value(0).toString());
    }

    // 2. 清空数据库 transfer_log 表
    QSqlQuery query;
    query.exec("DELETE FROM transfer_log");

    // 3. 删除接收到的文件（在程序运行目录下）
    for (const QString &fname : fileNames) {
        QFile f(fname);
        if (f.exists()) {
            f.remove();
        }
    }

    // 4. 刷新表格
    refreshLogTable();

    ui->serverLogTextEdit->append("传输记录已清除");
}

// ===== 数据库 =====
void MainWindow::initDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("transfer.db");

    if (!db.open()) {
        ui->serverLogTextEdit->append("数据库打开失败：" + db.lastError().text());
        return;
    }

    QSqlQuery query;
    query.exec(
        "CREATE TABLE IF NOT EXISTS transfer_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "type TEXT NOT NULL,"
        "sender TEXT NOT NULL,"
        "content TEXT,"
        "file_size INTEGER,"
        "time DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")"
    );
    ui->serverLogTextEdit->append("数据库初始化完成");
}

void MainWindow::logMessage(const QString &sender, const QString &content)
{
    QSqlQuery query;
    query.prepare("INSERT INTO transfer_log (type, sender, content) VALUES ('message', ?, ?)");
    query.addBindValue(sender);
    query.addBindValue(content);
    query.exec();
}

void MainWindow::logFile(const QString &sender, const QString &fileName, qint64 fileSize)
{
    QSqlQuery query;
    query.prepare("INSERT INTO transfer_log (type, sender, content, file_size) VALUES ('file', ?, ?, ?)");
    query.addBindValue(sender);
    query.addBindValue(fileName);
    query.addBindValue(fileSize);
    query.exec();
}