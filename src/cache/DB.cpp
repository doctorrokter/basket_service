/*
 * DB.cpp
 *
 *  Created on: Jan 13, 2018
 *      Author: doctorrokter
 */

#include "DB.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSettings>

#define DB_NAME "basket.db"

Logger DB::logger = Logger::getLogger("DB::Service");
SqlDataAccess* DB::m_pSda = 0;

DB::DB(QObject* parent) : QObject(parent) {
    QSettings qsettings;

    QString dbDirPath = QDir::currentPath() + "/data/cache";
    QString dbpath = dbDirPath + "/" + QString(DB_NAME);
    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName(dbpath);
    m_database.open();

    m_pSda = new SqlDataAccess(dbpath, this);
}

DB::~DB() {
    m_database.close();
    m_pSda->deleteLater();
}

QVariant DB::execute(const QString& query) {
//    logger.debug(query);
    return m_pSda->execute(query);
}

QVariant DB::execute(const QString& query, const QVariantMap& values) {
//    logger.debug(query);
//    logger.debug(values);
    return m_pSda->execute(query, values);
}
