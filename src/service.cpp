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

#include "service.hpp"

#include <bb/Application>
#include <bb/platform/Notification>
#include <bb/platform/NotificationDefaultApplicationSettings>
#include <bb/system/InvokeManager>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QDataStream>
#include <QSettings>
#include <QUrl>

#include <QTimer>

#define AUTOLOAD_CAMERA_FILES_ENABLED "autoload.camera.files.enabled"
#define AUTOLOAD_CAMERA_FILES_DISABLED "autoload.camera.files.disabled"
#define CAMERA_DIR "/shared/camera"
#define INDEX_FILE_PLACE "/data/index"
#define ACCESS_TOKEN_KEY "dropbox.access_token"
#define DROPBOX_UPLOAD_SIZE 157286400 // 150 MB
#define UPLOAD_SIZE (1048576 / 2) // 0.5 MB

using namespace bb::platform;
using namespace bb::system;

Logger Service::logger = Logger::getLogger("Service");

Service::Service() :
        QObject(),
        m_notify(new Notification(this)),
        m_invokeManager(new InvokeManager(this)),
        m_pWatcher(new QFileSystemWatcher(this)),
        m_pQdropbox(new QDropbox(this)),
        m_pDb(0),
        m_pCache(0),
        m_autoload(false) {

    QCoreApplication::setOrganizationName("mikhail.chachkouski");
    QCoreApplication::setApplicationName("Basket");

    m_invokeManager->connect(m_invokeManager, SIGNAL(invoked(const bb::system::InvokeRequest&)), this, SLOT(handleInvoke(const bb::system::InvokeRequest&)));

    bool res = QObject::connect(m_pWatcher, SIGNAL(directoryChanged(const QString&)), this, SLOT(onDirectoryChanged(const QString&)));
    Q_ASSERT(res);
    res = QObject::connect(m_pWatcher, SIGNAL(fileChanged(const QString&)), this, SLOT(onFileChanged(const QString&)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(folderCreated(QDropboxFile*)), this, SLOT(onFolderCreated(QDropboxFile*)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(error(QNetworkReply::NetworkError, const QString&)), this, SLOT(onError(QNetworkReply::NetworkError, const QString&)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(uploadSessionStarted(const QString&, const QString&)), this, SLOT(onUploadSessionStarted(const QString&, const QString&)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(uploadSessionAppended(const QString&)), this, SLOT(onUploadSessionAppended(const QString&)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(uploadSessionFinished(QDropboxFile*)), this, SLOT(onUploadSessionFinished(QDropboxFile*)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(uploaded(QDropboxFile*)), this, SLOT(onUploaded(QDropboxFile*)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(urlSaved()), this, SLOT(onUrlSaved()));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(uploadFailed(const QString&)), this, SLOT(onUploadFailed(const QString&)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(jobStatusChecked(const UnshareJobStatus&)), this, SLOT(onJobStatusChecked(const UnshareJobStatus&)));
    Q_ASSERT(res);
    res = QObject::connect(m_pQdropbox, SIGNAL(metadataReceived(QDropboxFile*)), this, SLOT(onMetadataReceived(QDropboxFile*)));
    Q_ASSERT(res);
    res = QObject::connect(this, SIGNAL(filesAdded(const QString&, const QStringList&)), this, SLOT(onFilesAdded(const QString&, const QStringList&)));
    Q_ASSERT(res);
    Q_UNUSED(res);

    NotificationDefaultApplicationSettings settings;
    settings.setPreview(NotificationPriorityPolicy::Allow);
    settings.apply();

    m_notify->setTitle("Basket");
    m_notify->setBody("Started in background");

    bb::system::InvokeRequest request;
    request.setTarget("chachkouski.Basket");
    request.setAction("bb.action.START");
    m_notify->setInvokeRequest(request);

    onTimeout();

    QSettings qsettings;
    qsettings.setValue("headless.started", true);
    qsettings.sync();
    m_autoload = qsettings.value("autoload.camera.files", false).toBool();
    m_pQdropbox->setAccessToken(qsettings.value(ACCESS_TOKEN_KEY, "").toString());
    m_pWatcher->addPath(qsettings.fileName());

    m_mode = Default;

    logger.debug("Constructor called");
}

Service::~Service() {
    m_pWatcher->deleteLater();
    m_invokeManager->deleteLater();
    m_notify->deleteLater();
    m_pQdropbox->deleteLater();
    m_pDb->deleteLater();
    m_pCache->deleteLater();
}

void Service::handleInvoke(const bb::system::InvokeRequest& request) {
    QString a = request.action();
    logger.debug("Invoke action: " + a);
    if (a.compare("chachkouski.BasketService.RESET") == 0) {
        triggerNotification();
    } else if (a.compare("chachkouski.BasketService.START") == 0) {
        initCache();
    } else if (a.compare("chachkouski.BasketService.UPLOAD_FILES") == 0) {
        m_mode = SharingFiles;

        QByteArray data = request.data();
        QDataStream in(&data, QIODevice::ReadOnly);
        QVariantMap map;
        in >> map;

        QString path = map.value("path").toString();
        QVariantList files = map.value("files").toList();

        QString message = "Files will be uploaded:\n";

        foreach(QVariant var, files) {
            QUrl url = QUrl::fromEncoded(var.toString().toAscii());
            QString localPath = url.toString();
            QString name = m_fileUtil.filename(localPath);
            message.append("- " + name + "\n");

            QDropboxUpload upload(localPath, path + "/" + name, this);
            upload.setUploadSize(DROPBOX_UPLOAD_SIZE);
            m_uploads.enqueue(upload);
            if (m_uploads.size() == 1) {
                processUploadsQueue();
            }
        }

        m_notify->setBody(message);
        triggerNotification();
    } else if (a.compare("chachkouski.BasketService.SAVE_URL") == 0) {
        m_mode = SharingUrl;

        QByteArray data = request.data();
        QDataStream in(&data, QIODevice::ReadOnly);
        QVariantMap map;
        in >> map;

        QString path = map.value("path").toString();
        QUrl url = QUrl::fromEncoded(map.value("url").toString().toAscii());

        m_pQdropbox->saveUrl(path, url.toString());
    } else if (a.compare("chachkouski.BasketService.CHECK_JOB_STATUS") == 0) {
        QByteArray data = request.data();
        QDataStream in(&data, QIODevice::ReadOnly);
        QVariantMap map;
        in >> map;

        UnshareJobStatus status;
        status.fromMap(map.value("status").toMap());
        QString path = map.value("path").toString();

        m_sharedFolderIds[status.sharedFolderId] = path;
        m_jobStatuses[status.asyncJobId] = status;
        m_pQdropbox->checkJobStatus(status.asyncJobId);
    } else {
        initCache();
    }
}

void Service::onJobStatusChecked(const UnshareJobStatus& status) {
    if (status.status == UnshareJobStatus::Complete) {
        UnshareJobStatus oldStatus = m_jobStatuses.value(status.asyncJobId);
        m_pQdropbox->getMetadata(m_sharedFolderIds.value(oldStatus.sharedFolderId));
        m_sharedFolderIds.remove(oldStatus.sharedFolderId);
        m_jobStatuses.remove(status.asyncJobId);
        logger.info("Job status complete: " + status.asyncJobId);
    } else {
        m_pQdropbox->checkJobStatus(status.asyncJobId);
    }
}

void Service::onMetadataReceived(QDropboxFile* file) {
    logger.debug(file->toMap());
    m_pCache->update(file);
    file->deleteLater();
}

void Service::onUrlSaved() {
    m_mode = Default;
    m_notify->setBody("URL saved!");
    triggerNotification();
}

void Service::triggerNotification() {
    // Timeout is to give time for UI to minimize
    QTimer::singleShot(2000, this, SLOT(onTimeout()));
}

void Service::onTimeout() {
    Notification::clearEffectsForAll();
    Notification::deleteAllFromInbox();
    m_notify->notify();
}

void Service::onDirectoryChanged(const QString& path) {
    logger.debug("Dir changed: " + path);
    updateIndex(path, m_paths.value(path));
}

void Service::onFileChanged(const QString& path) {
    logger.debug("File changed: " + path);
    if (path.contains("Basket.conf")) {
        QSettings qsettings;
        m_autoload = qsettings.value("autoload.camera.files", false).toBool();

        QString token = qsettings.value(ACCESS_TOKEN_KEY).toString();
        m_pQdropbox->setAccessToken(token);
        if (token.isEmpty()) {
            m_autoload = false;
            qsettings.setValue("autoload.camera.files", m_autoload);
            qsettings.sync();
        }

        switchAutoload();
    }
}

void Service::switchAutoload() {
    QString dir = QDir::currentPath() + CAMERA_DIR;
    QString name = dir.split("/").last();
    if (m_autoload) {
        m_pQdropbox->createFolder("/Camera");
        m_pWatcher->addPath(dir);
        m_paths[dir] = name;
        removeIndex(name);
        createIndex(dir, name);
    } else {
        m_pWatcher->removePath(dir);
        removeIndex(m_paths.value(dir));
        m_paths.remove(dir);
    }
}

void Service::updateIndex(const QString& path, const QString& name) {
    QString indexPath = QDir::currentPath() + INDEX_FILE_PLACE + "/" + name + ".txt";
    int count = 0;

    QFile index(indexPath);
    index.open(QIODevice::ReadWrite | QIODevice::Text);

    QStringList added;
    QDirIterator it(path, QDir::NoDotAndDotDot | QDir::Files);
    QTextStream in(&index);

    while(it.hasNext()) {
        QString p = it.next();
        count++;
        in.seek(0);
        bool found = false;
        while(!in.atEnd()) {
            QString line = in.readLine();
            if (line.contains(p)) {
                found = true;
                break;
            }
        }

        if (!found) {
            added.append(p);
        }
    }

    if (added.size()) {
        QTextStream out(&index);
        foreach(QString newPath, added) {
            out << newPath << endl;
        }
        emit filesAdded(path, added);
    }
    index.close();

    logger.info("Index file updated: " + name + ", entries count: " + QString::number(count) + ", file size: " + QString::number(index.size()));
}

void Service::createIndex(const QString& path, const QString& name) {
    QDir dir(QDir::currentPath() + INDEX_FILE_PLACE);
    if (!dir.exists()) {
        dir.mkpath(QDir::currentPath() + INDEX_FILE_PLACE);
    }

    QString indexPath = QDir::currentPath() + INDEX_FILE_PLACE + "/" + name + ".txt";
    int count = 0;

    QFile index(indexPath);
    index.open(QIODevice::WriteOnly | QIODevice::Text);

    QDirIterator it(path, QDir::NoDotAndDotDot | QDir::Files);
    QTextStream out(&index);
    while(it.hasNext()) {
        count++;
        out << it.next() << endl;
    }
    index.close();

    logger.info("Index file created: " + name + ", entries count: " + QString::number(count) + ", file size: " + QString::number(index.size()));
}

void Service::removeIndex(const QString& name) {
    QString indexPath = QDir::currentPath() + INDEX_FILE_PLACE + "/" + name + ".txt";
    QFile index(indexPath);
    if (index.exists()) {
        index.remove();
    }
    logger.debug("Index file removed: " + name + ".txt");
}

void Service::onFolderCreated(QDropboxFile* folder) {
    logger.info("Folder created: " + folder->getName());
    folder->deleteLater();
}

void Service::onError(QNetworkReply::NetworkError e, const QString& errorString) {
    logger.error(errorString);
    logger.error(e);
    dequeue();
}

void Service::onUploadProgress(const QString& path, qint64 loaded, qint64 total) {
    logger.debug("Progress for " + path + ": " + QString::number(loaded) + ", total: " + QString::number(total));
}

void Service::onUploadSessionStarted(const QString& remotePath, const QString& sessionId) {
    QDropboxUpload& upload = m_uploads.head();
    upload
        .setSessionId(sessionId)
        .increment();
    Q_UNUSED(remotePath);
    processUploadsQueue();
}

void Service::onUploadSessionAppended(const QString& sessionId) {
    QDropboxUpload& upload = m_uploads.head();
    upload.increment();
    processUploadsQueue();
    Q_UNUSED(sessionId);
}

void Service::onUploadSessionFinished(QDropboxFile* file) {
    dequeue(file);
}

void Service::onUploaded(QDropboxFile* file) {
    dequeue(file);
 }

void Service::processUploadsQueue() {
    QDropboxUpload& upload = m_uploads.head();
    if (upload.getSize() == 0) {
        upload.resize();
    }
    if (upload.getSize() <= upload.getUploadSize()) {
        QFile* file = new QFile(upload.getPath());
        m_pQdropbox->upload(file, upload.getRemotePath());
    } else {
        if (upload.isNew()) {
            upload.setUploadSize(UPLOAD_SIZE);
            m_pQdropbox->uploadSessionStart(upload.getRemotePath(), upload.next());
        } else {
            qint64 offset = upload.getOffset();
            if (upload.lastPortion()) {
                m_pQdropbox->uploadSessionFinish(upload.getSessionId(), upload.next(), offset, upload.getRemotePath());
            } else {
                m_pQdropbox->uploadSessionAppend(upload.getSessionId(), upload.next(), offset);
            }
        }
    }
}

void Service::dequeue(QDropboxFile* file) {
    if (file != 0) {
        logger.info("File uploaded: " + file->getPathDisplay());
        logger.info("File size: " + QString::number(file->getSize()));
        file->deleteLater();
    }

    if (m_uploads.size()) {
        m_uploads.dequeue();
        logger.debug("upload dequeued");
    }

    if (m_uploads.size()) {
        processUploadsQueue();
    } else {
        if (m_mode == SharingFiles) {
            m_notify->setBody("File(s) uploaded!");
            triggerNotification();
        }

        m_mode = Default;
    }
}

void Service::onFilesAdded(const QString& path, const QStringList& files) {
    foreach(QString localPath, files) {
        QString name = m_fileUtil.filename(localPath);
        logger.debug("Will upload file " + name + " to /Camera/");

        QDropboxUpload upload(localPath, "/Camera/" + name, this);
        m_uploads.enqueue(upload);
        if (m_uploads.size() == 1) {
            QTimer::singleShot(5000, this, SLOT(processUploadsQueue()));
        }
    }
    Q_UNUSED(path);
}

void Service::onUploadFailed(const QString& reason) {
    logger.error(reason);
    dequeue();
}

void Service::initCache() {
    if (m_pDb == 0) {
        m_pDb = new DB(this);
    }

    if (m_pCache == 0) {
        m_pCache = new QDropboxCache(this);
    }
}
