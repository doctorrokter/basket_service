APP_NAME = BasketService

CONFIG += qt warn_on

include(config.pri)
include($$quote($$_PRO_FILE_PWD_)/../qdropbox/static.pri)

LIBS += -lbb -lbbsystem -lbbplatform -lbbnetwork -lbbdata

QT += network core sql