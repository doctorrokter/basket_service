/*
 * DB.hpp
 *
 *  Created on: Jan 13, 2018
 *      Author: doctorrokter
 */

#ifndef DB_HPP_
#define DB_HPP_

#include <QtCore/QObject>
#include <bb/data/SqlDataAccess>
#include <QtSql/QtSql>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include "../Logger.hpp"

using namespace bb::data;

class DB: public QObject {
    Q_OBJECT
public:
    DB(QObject* parent = 0);
    virtual ~DB();

    static QVariant execute(const QString& query);
    static QVariant execute(const QString& query, const QVariantMap& data);
    static QVariant execute(const QString& query, const QVariantList& data);

private:
    static SqlDataAccess* m_pSda;
    static Logger logger;

    QSqlDatabase m_database;
};

#endif /* DB_HPP_ */
