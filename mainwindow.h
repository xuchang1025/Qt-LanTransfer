#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QWidget>
#include <QDebug>
#include <QFileDialog>
#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlTableModel>
#include <QTableView>

#include <QTcpServer>
#include <QTcpSocket>

class FileSender;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStartListenClicked();    // 启动/停止服务端监听
    void onNewConnection();         // 新客户端连接到来时调用
    void onConnectClicked();        // 客户端连接服务端
    void onSendClicked();           // 发送文本消息
    void onReplyClicked();          // 服务端回复消息
    void onSelectFileClicked();     // 选择要发送的文件
    void onSendFileClicked();       // 发送文件
    void onClearHistoryClicked();   // 清除传输记录

private:
    Ui::MainWindow *ui;

    // ==== 网络连接 ====
    QTcpServer *tcpServer;           // 服务端监听器
    QTcpSocket *clientSocket;        // 客户端使用的 socket
    QTcpSocket *serverClientSocket;    // 服务端与客户端的通信 socket

    QString selectedFilePath;        // 当前选择的文件路径

    // ==== 数据库 ====
    void initDatabase();             // 初始化数据库
    void logMessage(const QString &sender, const QString &content);  // 记录文本消息
    void logFile(const QString &sender, const QString &fileName, qint64 fileSize);  // 记录文件传输

    // ==== 传输历史（Model/View 架构） ====
    QSqlTableModel *logModel;        // 模型：映射数据库表到 UI
    QTableView *logTableView;        // 视图：显示传输历史表格
    void setupTransferHistory();     // 设置表格视图
    void refreshLogTable();          // 刷新表格数据

    // ==== 多线程文件发送 ====
    FileSender *fileSender;          // 文件发送器工作对象

    // ==== 界面美化 ====
    void setupStyle();               // 应用 QSS 样式表

    // ==== 文件接收状态（服务端） ====
    QByteArray recvBuffer;           // 接收缓冲区
    bool waitingForFile = false;     // 是否正在等待文件
    QString recvFileName;            // 接收的文件名
    qint64 recvFileSize = 0;         // 接收的文件大小
    int recvTotalChunks = 0;         // 文件总片数
    int recvReceivedChunks = 0;      // 已接收片数
    QByteArray recvFileBuffer;       // 文件数据缓冲区

    // ==== 协议解析（服务端） ====
    void processBuffer();            // 处理接收缓冲区
    bool handleFileHeader();         // 处理文件头部信息
    bool handleChunk();              // 处理文件数据片
    void handleTextMessage();        // 处理文本消息
    };

#endif // MAINWINDOW_H
