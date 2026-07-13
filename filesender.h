#ifndef FILESENDER_H
#define FILESENDER_H

#include <QObject>
#include <QThread>
#include <QByteArray>
#include <QString>

// ===== 文件发送工作类 =====
// 负责在子线程中读取文件、切片，通过信号把数据传给主线程
// 主线程收到信号后写 socket，这样 UI 不会卡死
class FileSender : public QObject
{
    Q_OBJECT
public:
    explicit FileSender(QObject *parent = nullptr);
    ~FileSender();

    // 主线程调用：启动文件发送（内部通过信号槽触发子线程执行）
    void start(const QString &filePath);

signals:
    // 发送给主线程：FILE: 头部信息（主线程负责写 socket）
    void headerReady(const QString &fileName, qint64 fileSize, int totalChunks);
    // 发送给主线程：每片数据（index + 二进制数据）
    void chunkReady(int index, const QByteArray &chunkData);
    // 进度更新
    void progressUpdated(int current, int total);
    // 发送完成
    void finished(const QString &fileName, qint64 fileSize);
    // 发送出错
    void errorOccurred(const QString &message);

private slots:
    // 在子线程中执行：读取文件、切片、逐片发出 chunkReady
    void doSend(const QString &filePath);

private:
    QThread m_workerThread;
};

#endif // FILESENDER_H
