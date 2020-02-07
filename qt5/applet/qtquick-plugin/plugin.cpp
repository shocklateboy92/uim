#include "plugin.h"

#include <QtQml/QtQml>

#include "uim-socket.h"

void UimPlugin::registerTypes(const char* uri) {
    // Register our 'UimSocket' in qml engine
    qmlRegisterType<UimSocket>(uri, 1, 0, "UimSocket");
}
