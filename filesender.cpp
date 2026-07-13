#include "filesender.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

FileSender::FileSender(QObject *parent)
    : QObject(parent)
{
    // 把自身移动到子线程（之后所有槽函数都在子线程执行）
    moveToThread(&m_workerThread);

    // 线程结束时自动删除自己
    connect(&m_workerThread, &QThread::finished, this, &QObject::deleteLater);

    m_workerThread.start();
}

FileSender::~FileSender()
{
    m_workerThread.quit();
    m_workerThread.wait();
}

// 主线程调用：通过信号槽触发子线程执行
void FileSender::start(const QString &filePath)
{
    // 跨线程调用 doSend，在子线程中执行
    QMetaObject::invokeMethod(this, "doSend", Qt::QueuedConnection,
                              Q_ARG(QString, filePath));
}

// ===== 在子线程中执行：读取文件 + 切片 =====
void FileSender::doSend(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("无法打开文件：" + filePath);
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QString fileName = QFileInfo(filePath).fileName();
    qint64 fileSize = fileData.size();
    const qint64 chunkSize = 64 * 1024;
    int totalChunks = (fileSize + chunkSize - 1) / chunkSize;

    // 1. 通知主线程发送 FILE: 头部
    emit headerReady(fileName, fileSize, totalChunks);

    // 2. 逐片读取，通过信号发给主线程
    for (int i = 0; i < totalChunks; i++) {
        QByteArray chunk = fileData.mid(i * chunkSize, chunkSize);
        emit chunkReady(i, chunk);
        emit progressUpdated(i + 1, totalChunks);
    }

    // 3. 完成
    emit finished(fileName, fileSize);
}
