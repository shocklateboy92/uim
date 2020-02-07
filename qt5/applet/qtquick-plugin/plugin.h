#ifndef MYPLUGIN_H
#define MYPLUGIN_H

#include <QtQml/QQmlExtensionPlugin>

/**
 * @brief The MyPlugin class. Simple qml plugin example.
 */

class UimPlugin: public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")
public:

    /**
     * @brief registerTypes Overrided function that should register all
     * C++ classes exported by this plugin.
     * @param uri           Plugin uri.
     */
    void registerTypes(const char* uri) override;
};

#endif // MYPLUGIN_H
