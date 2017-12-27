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
#include "Logger.hpp"
#include <QFileSystemWatcher>
#include <QMap>
#include <qdropbox/QDropbox.hpp>
#include <qdropbox/QDropboxFile.hpp>
#include <qdropbox/QDropboxUpload.hpp>
#include <QQueue>
#include <QStringList>
#include "util/FileUtil.hpp"

#define UPLOAD_SIZE (1048576 / 2) // 0.5 MB

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

class Service: public QObject {
    Q_OBJECT
public:
    Service();
    virtual ~Service();

    Q_SIGNALS:
        void filesAdded(const QString& path, const QStringList& files);

private slots:
    void handleInvoke(const bb::system::InvokeRequest &);
    void onTimeout();
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);
    void onFolderCreated(QDropboxFile* folder);
    void onError(QNetworkReply::NetworkError e, const QString& errorString);
    void onUploadProgress(const QString& path, qint64 loaded, qint64 total);
    void onUploadSessionStarted(const QString& remotePath, const QString& sessionId);
    void onUploadSessionAppended(const QString& sessionId);
    void onUploadSessionFinished(QDropboxFile* file);
    void onUploaded(QDropboxFile* file);
    void processUploadsQueue();
    void onFilesAdded(const QString& path, const QStringList& files);

private:
    void triggerNotification();
    void switchAutoload();
    void updateIndex(const QString& path, const QString& name);
    void createIndex(const QString& path, const QString& name);
    void removeIndex(const QString& name);
    void dequeue(QDropboxFile* file = 0);

    bb::platform::Notification * m_notify;
    bb::system::InvokeManager * m_invokeManager;
    QFileSystemWatcher* m_pWatcher;
    QDropbox* m_pQdropbox;

    QQueue<QDropboxUpload> m_uploads;
    bool m_autoload;
    QMap<QString, QString> m_paths;
    FileUtil m_fileUtil;

    static Logger logger;
};

#endif /* SERVICE_H_ */
