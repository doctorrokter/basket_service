/*
 * Watcher.hpp
 *
 *  Created on: Dec 18, 2017
 *      Author: doctorrokter
 */

#ifndef WATCHER_HPP_
#define WATCHER_HPP_

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDirIterator>
#include "Logger.hpp"
#include <QTimer>

class Watcher: public QObject {
    Q_OBJECT
public:
    Watcher(QObject* parent = 0);
    virtual ~Watcher();

    void addPath(const QString& path);
    void unwatch(const QString& path);
    QStringList entryList(const QString& path);
    void sync();

    Q_SIGNALS:
        void filesAdded(const QString& path, const QStringList& addedEntries);
        void filesRemoved(const QString& path, const QStringList& removedEntries);

private slots:
    void onTimeout();

private:
    static Logger logger;

    QTimer* m_pTimer;
    QMap<QString, QString> m_paths;

    QStringList checkForAddedEntries(const QStringList& newEntries, const QStringList& oldEntries);
    QStringList checkForRemovedEntries(const QStringList& newEntries, const QStringList& oldEntries);
    void updateIndex(const QString& name, const QStringList& entries);
};

#endif /* WATCHER_HPP_ */
