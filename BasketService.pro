APP_NAME = BasketService

CONFIG += qt warn_on

include(config.pri)

LIBS += -lbb -lbbsystem -lbbplatform -lbbnetwork -lbbdata

QT += network xml