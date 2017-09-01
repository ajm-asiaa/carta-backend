/**
 *
 **/


#ifndef SESSION_DISPATCHER_H
#define SESSION_DISPATCHER_H

#include <QObject>
#include "core/IConnector.h"
#include "core/CallbackList.h"
#include "CartaLib/IRemoteVGView.h"
#include "DesktopConnector.h"

#include <QList>
#include <QByteArray>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

QT_FORWARD_DECLARE_CLASS(WebSocketClientWrapper)
QT_FORWARD_DECLARE_CLASS(QWebChannel)

class MainWindow;
class IView;

/// private info we keep with each view
/// unfortunately it needs to live as it's own class because we need to give it slots...
//class ViewInfo;

class SessionDispatcher : public QObject, public IConnector
{
    Q_OBJECT
public:

    /// constructor
    explicit SessionDispatcher();

    // implementation of IConnector interface
    virtual void initialize( const InitializeCallback & cb) override;

    virtual CallbackID addCommandCallback( const QString & cmd, const CommandCallback & cb) override;
    virtual CallbackID addStateCallback(CSR path, const StateChangedCallback &cb) override;


    //** will comment later
    virtual void setState(const QString& state, const QString & newValue) override;
    virtual QString getState(const QString&) override;
    virtual void registerView(IView * view) override;
    void unregisterView( const QString& viewName ) override;
    virtual qint64 refreshView( IView * view) override;
    virtual void removeStateCallback( const CallbackID & id) override;
    virtual Carta::Lib::IRemoteVGView * makeRemoteVGView( QString viewName) override;
    // Return the location where the state is saved.
    virtual QString getStateLocation( const QString& saveName ) const;

    //**

    // no use
    // void startWebSocketServer();
   // void pseudoJsSendCommandSlot(const QString &cmd, const QString & parameter);
   // void jsSendCommandSlot2();
   // void pseudoJsSendCommandSlot(uWS::WebSocket<uWS::SERVER> *ws, uWS::OpCode opCode,  const QString &cmd, const QString & parameter);
    // Qt's built-in WebSocket
   // QList<QWebSocket *> m_clients;
   // bool m_debug;
   // void pseudoJsSendCommandSlot(const QString &cmd, const QString & parameter, QWebSocket *pClient);
   //

    void startWebSocketChannel();
    QWebSocketServer *m_pWebSocketServer;
    WebSocketClientWrapper *m_clientWrapper;
    QWebChannel *m_channel;
     ~SessionDispatcher();
public slots:

    /// javascript calls this to set a state
    void jsSetStateSlot( const QString & key, const QString & value);
    /// javascript calls this to send a command
    void jsSendCommandSlot(const QString & sessionID, const QString & cmd, const QString & parameter);
    /// javascript calls this to let us know js connector is ready
    void newSessionCreatedSlot(const QString & sessionID);
    /// javascript calls this when view is resized
    void jsUpdateViewSlot(const QString & sessionID, const QString & viewName, int width, int height);
    /// javascript calls this when the view is refreshed
    void jsViewRefreshedSlot( const QString & viewName, qint64 id);
    /// javascript calls this on mouse move inside a view
    /// \deprecated
    void jsMouseMoveSlot( const QString & viewName, int x, int y);

    /// this is the callback for stateChangedSignal
    void stateChangedSlot( const QString & key, const QString & value);

    void jsCommandResultsSignalForwardSlot(const QString & sessionID, const QString & cmd, const QString & results);
    void jsViewUpdatedSignalForwardSlot(const QString & sessionID, const QString & viewName, const QString & img, qint64 id);
    void jsSendKeepAlive();

    // no use. Qt's built-in WebSocket
//    void onNewConnection();
//    void processTextMessage(QString message);
//    void processBinaryMessage(QByteArray message);
//    void socketDisconnected();


signals:

    // void closed();
    /// we emit this signal when state is changed (either by c++ or by javascript)
    /// we listen to this signal, and so does javascript
    /// our listener then calls callbacks registered for this value
    /// javascript listener caches the new value and also calls registered callbacks
    void stateChangedSignal( const QString & key, const QString & value);
    /// we emit this signal when command results are ready
    /// javascript listens to it
    void jsCommandResultsSignal(const QString & sessionID, const QString & cmd, const QString & results);
    /// emitted by c++ when we want javascript to repaint the view
    void jsViewUpdatedSignal(const QString & sessionID, const QString & viewName, const QString & img, qint64 id);

    // for new arch, forward js->cpp's sessionhDispatcher -> others
    void startViewerSignal(const QString & sessionID);
    void jsSendCommandSignal(const QString & sessionID, const QString &cmd, const QString & parameter);
    void jsUpdateViewSignal(const QString & sessionID, const QString & viewName, int width, int height);
public:

    //** will comment later until the end
    typedef std::vector<CommandCallback> CommandCallbackList;
    std::map<QString,  CommandCallbackList> m_commandCallbackMap;
    // list of callbacks
    typedef CallbackList<CSR, CSR> StateCBList;
    /// for each state we maintain a list of callbacks
    std::map<QString, StateCBList *> m_stateCallbackList;
    /// IDs for command callbacks
    CallbackID m_callbackNextId;
    /// private info we keep with each view
    struct ViewInfo;

    /// map of view names to view infos
    std::map< QString, ViewInfo *> m_views;

    ViewInfo * findViewInfo(const QString &viewName);

    virtual void refreshViewNow(IView *view);

    IConnector* getConnectorInMap(const QString & sessionID);

    void setConnectorInMap(const QString & sessionID, IConnector *connector);

    /// @todo move as may of these as possible to protected section

protected:

    InitializeCallback m_initializeCallback;
    std::map< QString, QString > m_state;
//    std::map<QString,  DesktopConnector*> clientList;
    std::map<QString,  IConnector*> clientList;

private:
    QMutex mutex;
};


#endif // SESSION_DISPATCHER_H
