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

    m_freeSlots.release(4);
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

    // 用 size() 预知文件大小，不把内容读进内存
    // 原实现用 readAll()，内存占用 = 文件大小，传 1GB 直接 OOM
    const qint64 fileSize = file.size();
    const int chunkSize = 64 * 1024;
    const int totalChunks = int((fileSize + chunkSize - 1) / chunkSize);

    if (fileSize == 0) {
        emit errorOccurred("文件为空，未发送：" + filePath);
        return;
    }

    const QString fileName = QFileInfo(filePath).fileName();

    // 1. 通知主线程发送 FILE: 头部
    emit headerReady(fileName, fileSize, totalChunks);

    // 2. 流式读取：一次只驻留 64KB，内存占用与文件大小无关
    int index = 0;
    while (!file.atEnd()) {
        m_freeSlots.acquire();
        QByteArray chunk = file.read(chunkSize);
        if (chunk.isEmpty())
            break;
        emit chunkReady(index, chunk);
        ++index;
        emit progressUpdated(index, totalChunks);
    }

    // 3. 完成
    emit finished(fileName, fileSize);
}
