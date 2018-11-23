/***
 * Main class that manages the data state for the views.
 *
 */

#pragma once

#include "State/StateInterface.h"
#include "State/ObjectManager.h"
#include <QVector>
#include <QObject>

namespace Carta {

namespace Data {

class Controller;
class DataLoader;
//class Colormap;
//class Statistics;
class ViewPlugins;

class ViewManager : public QObject, public Carta::State::CartaObject {

    Q_OBJECT

public:
    /**
     * Return the unique server side id of the object with the given name and index in the
     * layout.
     * @param pluginName an identifier for the kind of object.
     * @param index an index in the case where there is more than one object of the given kind
     *      in the layout.
     */
    QString getObjectId( const QString& pluginName, int index, bool forceCreate = false );

    /**
     * Link a source plugin to a destination plugin.
     * @param sourceId the unique server-side id for the plugin that is the source of the link.
     * @param destId the unique server side id for the plugin that is the destination of the link.
     * @return an error message if the link does not succeed.
     */
//    QString linkAdd( const QString& sourceId, const QString& destId );

    /**
     * Remove a link from a source to a destination.
     * @param sourceId the unique server-side id for the plugin that is the source of the link.
     * @param destId the unique server side id for the plugin that is the destination of the link.
     * @return an error message if the link does not succeed.
     */
//    QString linkRemove( const QString& sourceId, const QString& destId );

    /**
     * Return the number of controllers (image views).
     * @return - the controller count.
     */
    int getControllerCount() const;

    /**
     * Return the number of colormap views.
     * @return - the colormap count.
     */
    int getColormapCount() const;

    /**
     * Load the file into the controller with the given id.
     * @param fileName a locater for the data to load.
     * @param objectId the unique server side id of the controller which is
     * responsible for displaying the file.
     * @param success - set to true if the file was successfully loaded; false otherwise.
     * @return - the identifier of the server-side object managing the image if the file
     *      was successfully loaded; otherwise, an error message.
     */
    QString loadFile( const QString& objectId, const QString& fileName, bool* success);


    /**
     * Replace the destination plug-in identified by its type and index with the source
     * plug-in identified by its type and index.
     * @param sourcePlugin - an identifier for the plug-in to move.
     * @param sourcePluginIndex - the index of the plug-in to move within plug-ins of its type.
     * @param destPlugin - an identifier for the plug-in to replace.
     * @param destPluginIndex - the index of the plug-in to replace within plug-ins of its type.
     * @return an error if there was a problem doing the move; an empty string otherwise.
     */
    QString moveWindow( const QString& sourcePlugin, int sourcePluginIndex,
            const QString& destPlugin, int destPluginIndex );

    static const QString CLASS_NAME;

    /**
     * Destructor.
     */
    virtual ~ViewManager();

    QString dataLoaded(const QString & params);
    QString registerView(const QString & params);

private slots:
    void _pluginsChanged( const QStringList& names, const QStringList& oldNames );

private:
    ViewManager( const QString& path, const QString& id);
    class Factory;
    void _clearColormaps( int startIndex, int upperBound );
    void _clearControllers( int startIndex, int upperBound );
    void _clearStatistics( int startIndex, int upperBound );

    /**
     * Given the plugin and the index of the plugin among plugins of its type, find the index of the plugin
     * in the QStringList.
     * @param sourcePlugin - an identifier for the type of plugin.
     * @param pluginIndex - the index of the plugin amoung plugins of matching type.
     * @param plugins - a QStringList of plugins to search.
     * @return the index of the identifier plugin in the QStringList.
     */
    int _findListIndex( const QString& sourcePlugin, int pluginIndex, const QStringList& plugins ) const;

    void _initCallbacks();

    //Returns whether or not there are any source objects of the given sourceName already linked to the
    //destination object.
    QString _isDuplicateLink( const QString& sourceName, const QString& destId ) const;

    QString _makePluginList();
//    QString _makeColorMap( int index );
    QString _makeController( int index );
//    QString _makeStatistics( int index );

    void _makeDataLoader();

    /**
     * Move the identified plugin from the oldIndex to the newIndex.
     */
    void _moveView( const QString& plugin, int oldIndex, int newIndex );

    /**
     * Written because there is no guarantee what order the javascript side will use
     * to create view objects.  When there are linked views, the links may not get
     * recorded if one object is to be linked with one not yet created.  This flushes
     * the state and gives the object a second chance to establish their links.
     */
    void _refreshState();

    //A list of Controllers requested by the client.
    QList <Controller* > m_controllers;

    //Colormap
//    QList<Colormap* >m_colormaps;

    //Statistics
//    QList<Statistics* > m_statistics;

    static bool m_registered;
    DataLoader* m_dataLoader;
    ViewPlugins* m_pluginsLoaded;

    const static QString SOURCE_ID;
    const static QString SOURCE_PLUGIN;
    const static QString SOURCE_LOCATION_ID;
    const static QString DEST_ID;
    const static QString DEST_PLUGIN;
    const static QString DEST_LOCATION_ID;

    ViewManager( const ViewManager& other);
    ViewManager& operator=( const ViewManager& other );
};

}
}
