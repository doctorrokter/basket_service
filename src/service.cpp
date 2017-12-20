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

#include <QTimer>

using namespace bb::platform;
using namespace bb::system;

#define CAMERA_DIR "/shared/camera"
#define ACCESS_TOKEN "u_XewBWc388AAAAAAAAF9Xc0lW_rhLW1dbzA_XoRYeGEi_6iazRrv5LMmxbJGZ0W"

Logger Service::logger = Logger::getLogger("Service");

Service::Service() :
        QObject(),
        m_notify(new Notification(this)),
        m_invokeManager(new InvokeManager(this)),
        m_pQdropbox(new QDropbox(this)) {

    m_invokeManager->connect(m_invokeManager, SIGNAL(invoked(const bb::system::InvokeRequest&)),
            this, SLOT(handleInvoke(const bb::system::InvokeRequest&)));

    NotificationDefaultApplicationSettings settings;
    settings.setPreview(NotificationPriorityPolicy::Allow);
    settings.apply();

    m_notify->setTitle("Basket Service");
    m_notify->setBody("Basket service requires attention");

    bb::system::InvokeRequest request;
    request.setTarget("com.example.Basket");
    request.setAction("bb.action.START");
    m_notify->setInvokeRequest(request);

    QString watchCameraPath = QDir::currentPath() + CAMERA_DIR;
    m_watcher.addPath(watchCameraPath);

    bool res = QObject::connect(m_pQdropbox, SIGNAL(folderCreated(QDropboxFile*)), this, SLOT(onFolderCreated(QDropboxFile*)));
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
    res = QObject::connect(&m_watcher, SIGNAL(filesAdded(const QString&, const QStringList&)), this, SLOT(onFilesAdded(const QString&, const QStringList&)));
    Q_UNUSED(res);

    m_pQdropbox->setAccessToken(ACCESS_TOKEN);
    m_pQdropbox->createFolder("/Camera");
}

Service::~Service() {
    logger.debug("Destroy");
    m_pQdropbox->deleteLater();
    m_invokeManager->deleteLater();
    m_notify->deleteLater();
}

void Service::handleInvoke(const bb::system::InvokeRequest& request) {
    if (request.action().compare("com.example.BasketService.RESET") == 0) {
        triggerNotification();
    }
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

void Service::onFolderCreated(QDropboxFile* folder) {
    logger.info("Folder created: " + folder->getName());
    folder->deleteLater();
}

void Service::onError(QNetworkReply::NetworkError e, const QString& errorString) {
    logger.error(errorString);
    logger.error(e);
    dequeue();
}

void Service::onFilesAdded(const QString& path, const QStringList& addedEntries) {
    logger.debug("Dir changed: " + path);
    foreach(QString name, addedEntries) {
        logger.debug("Will upload file " + name + " to /Camera/");

        QString localPath = path + "/" + name;

        Upload upload(localPath, "/Camera/" + name, this);
        m_uploads.enqueue(upload);
        if (m_uploads.size() == 1) {
            processUploadsQueue();
        }
    }
}

void Service::processUploadsQueue() {
    Upload& upload = m_uploads.head();
    if (upload.size == 0) {
        upload.resize();
    }
    qint64 offset = upload.offset;
    if (upload.size <= UPLOAD_SIZE) {
        QFile* file = new QFile(upload.path);
        m_pQdropbox->upload(file, upload.remotePath);
    } else {
        if (upload.isNew()) {
            m_pQdropbox->uploadSessionStart(upload.remotePath, upload.next());
        } else {
            if (upload.lastPortion()) {
                m_pQdropbox->uploadSessionFinish(upload.sessionId, upload.next(), offset, upload.remotePath);
            } else {
                m_pQdropbox->uploadSessionAppend(upload.sessionId, upload.next(), offset);
            }
        }
    }
}

void Service::onUploadSessionStarted(const QString& remotePath, const QString& sessionId) {
    Upload& upload = m_uploads.head();
    upload.sessionId = sessionId;
    upload.increment();
    Q_UNUSED(remotePath);
    processUploadsQueue();
}

void Service::onUploadSessionAppended(const QString& sessionId) {
    Upload& upload = m_uploads.head();
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
    }
}

void Service::onUploadProgress(const QString& path, qint64 loaded, qint64 total) {
    logger.debug("Progress for " + path + ": " + QString::number(loaded) + ", total: " + QString::number(total));
}