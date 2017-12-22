/*
 * Watcher.cpp
 *
 *  Created on: Dec 18, 2017
 *      Author: doctorrokter
 */

#include "Watcher.hpp"

#define INDEX_FILE_PLACE "/shared/misc/"
#define INTERVAL 60000

Logger Watcher::logger = Logger::getLogger("Watcher");

Watcher::Watcher(QObject* parent) : QObject(parent), m_timerId(0) {}

Watcher::~Watcher() {
    logger.debug("Destroy");
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

void Watcher::addPath(const QString& path) {
    if (!m_paths.contains(path)) {
        logger.info("Adding path for watching: " + path);

        QString name = path.split("/").last();
        m_paths[path] = name;

        updateIndex(name, entryList(path));

        if (m_timerId == 0) {
            m_timerId = startTimer(INTERVAL);
        }
    } else {
        logger.info("Path already in watching: " + path);
    }
}

void Watcher::unwatch(const QString& path) {
    m_paths.remove(path);
}

void Watcher::timerEvent(QTimerEvent* event) {
    sync();
    Q_UNUSED(event);
}

QStringList Watcher::entryList(const QString& path) {
    return QDir(path).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
}

QStringList Watcher::checkForAddedEntries(const QStringList& newEntries, const QStringList& oldEntries) {
    QStringList addedEntries;
    foreach(QString e, newEntries) {
        if (!oldEntries.contains(e)) {
            addedEntries.append(e);
        }
    }
    return addedEntries;
}

QStringList Watcher::checkForRemovedEntries(const QStringList& newEntries, const QStringList& oldEntries) {
    QStringList removedEntries;
    foreach(QString e, oldEntries) {
        if (!newEntries.contains(e)) {
            removedEntries.append(e);
        }
    }
    return removedEntries;
}

void Watcher::updateIndex(const QString& name, const QStringList& entries) {
    QFile file(QDir::currentPath() + INDEX_FILE_PLACE + name + ".txt");
    if (file.exists()) {
        file.remove();
    }
    file.open(QIODevice::ReadWrite | QIODevice::Text);


    QTextStream out(&file);
    foreach(QString f, entries) {
        out << f << endl;
    }


    file.close();
    logger.info("Index file updated: " + name + ", entries count: " + QString::number(entries.size()) + ", file size: " + QString::number(file.size()));
}

void Watcher::sync() {
    foreach(QString path, m_paths.keys()) {
        QStringList newEntries = entryList(path);

        QString name = m_paths.value(path);
        QFile file(QDir::currentPath() + INDEX_FILE_PLACE + name + ".txt");
        file.open(QIODevice::ReadWrite | QIODevice::Text);

        QStringList addedEntries;
        QTextStream in(&file);

        foreach(QString newE, newEntries) {
            in.seek(0);
            bool found = false;
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.contains(newE)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                addedEntries.append(newE);
            }
        }
        file.close();

        updateIndex(name, newEntries);
        if (addedEntries.size()) {
            emit filesAdded(path, addedEntries);
        }
    }
}
