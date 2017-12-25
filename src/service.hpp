/*
 * Copyright (c) 2013-2015 BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SERVICE_H_
#define SERVICE_H_

#include <QObject>
#include <QNetworkReply>
#include <qdropbox/QDropbox.hpp>
#include <qdropbox/QDropboxFile.hpp>
#include <QFileSystemWatcher>
#include "util/FileUtil.hpp"
#include <qdropbox/Logger.hpp>
#include <QStringList>
#include <QQueue>
#include <QFile>
#include "Watcher.hpp"
#include "communication/HeadlessCommunication.hpp"
#include <QTimer>

#define UPLOAD_SIZE (1048576 / 4)

namespace bb {
    class Application;
    namespace platform {
        class Notification;
    }
    namespace system {
        class InvokeManager;
        class InvokeRequest;
    }
}

struct Upload : public QObject {
    Upload(const QString& path, const QString& remotePath, QObject* parent = 0) : QObject(parent), offset(0), path(path), remotePath(remotePath), sessionId("") {
        size = 0;
    }

    Upload(const Upload& upload) : QObject(upload.parent()) {
        swap(upload);
    }

    Upload& operator=(const Upload& upload) {
        swap(upload);
        return *this;
    }

    qint64 offset;
    qint64 size;
    QString path;
    QString remotePath;
    QString sessionId;

    void swap(const Upload& upload) {
        offset = upload.offset;
        path = upload.path;
        remotePath = upload.remotePath;
        sessionId = upload.sessionId;
        resize();
    }

    void resize() {
        QFile file(path);
        size = file.size();
    }

    void increment() {
        offset += UPLOAD_SIZE;
    }

    bool isNew() {
        return sessionId.compare("") == 0;
    }

    bool started() {
        return sessionId.compare("") != 0;
    }

    bool lastPortion() {
        return size <= (offset + UPLOAD_SIZE);
    }

    QByteArray next() {
        QByteArray data;
        QFile file(path);
        bool res = file.open(QIODevice::ReadOnly);
        if (res) {
            if (offset == 0) {
                data = file.read(UPLOAD_SIZE);
            } else {
                file.seek(offset);
                data = file.read(UPLOAD_SIZE);
            }
            file.close();
        } else {
            qDebug() << "File didn't opened!!!" << endl;
        }
        return data;
    }
};

class Service: public QObject {
    Q_OBJECT
public:
    Service();
    virtual ~Service();
private slots:
    void handleInvoke(const bb::system::InvokeRequest &);
    void onTimeout();
    void onFolderCreated(QDropboxFile* folder);
    void onError(QNetworkReply::NetworkError e, const QString& errorString);
    void onUploadProgress(const QString& path, qint64 loaded, qint64 total);
    void onUploadSessionStarted(const QString& remotePath, const QString& sessionId);
    void onUploadSessionAppended(const QString& sessionId);
    void onUploadSessionFinished(QDropboxFile* file);
    void onUploaded(QDropboxFile* file);
    void processUploadsQueue();
    void onFilesAdded(const QString& path, const QStringList& addedEntries);
    void closeCommunication();
    void onConnectedWithUI();
    void onCommand(const QString& command);

private:
    void triggerNotification();
    void dequeue(QDropboxFile* file = 0);
    void switchAutoload();
    void establishCommunication();

    static Logger logger;

    bb::platform::Notification * m_notify;
    bb::system::InvokeManager * m_invokeManager;

    QQueue<Upload> m_uploads;

    QDropbox* m_pQdropbox;
    HeadlessCommunication* m_pCommunication;
    Watcher* m_pWatcher;
    QTimer* m_pQueueWatcher;
    FileUtil m_fileUtil;

    bool m_autoload;
};

#endif /* SERVICE_H_ */
