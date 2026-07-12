#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QWidget>
#include <QDebug>
#include <QFileDialog>
#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>


// QTcpServer：服务端监听类，负责监听端口，等待客户端连接
#include <QTcpServer>

// QTcpSocket：通信类，客户端和服务端真正收发数据都靠它
#include <QTcpSocket>

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
    // 点击“启动监听”按钮后执行
    void onStartListenClicked();

    // 有新的客户端连接进来时执行
    void onNewConnection();

    //客户端
    void onConnectClicked();

    void onSendClicked();

    //新增：客户端回复
    void onReplyClicked();

    //新增：文件传输
    void onSelectFileClicked();

    void onSendFileClicked();

private:
    Ui::MainWindow *ui;

    // 服务端对象：负责监听端口，不直接负责读写数据
    QTcpServer *tcpServer;
    QTcpSocket *clientSocket;
    QTcpSocket *serverClientSocket;

    //文件
    QString selectedFilePath;
    void initDatabase();           // 初始化数据库和表
    void logMessage(const QString &sender, const QString &content);  // 记录文本消息
    void logFile(const QString &sender, const QString &fileName, qint64 fileSize); // 记录文件传输

    //文件接收状态（服务区）
    QByteArray recvBuffer;          // TCP 接收缓冲区
    bool waitingForFile = false;    // 是否正在接收文件
    QString recvFileName;
    qint64 recvFileSize = 0;        //接收文件大小
    int recvTotalChunks = 0;        //被接收文件切片数量
    int recvReceivedChunks = 0;
    QByteArray recvFileBuffer;      // 文件数据缓冲区

    //协议解析（服务区）
    void processBuffer();           // 循环解析 recvBuffer
    bool handleFileHeader();        // 处理 FILE: 头部
    bool handleChunk();             // 处理 CHUNK: 分片
    void handleTextMessage();       // 处理文本消息




};

#endif // MAINWINDOW_H