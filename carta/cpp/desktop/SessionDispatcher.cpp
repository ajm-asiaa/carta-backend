/**
 *
 **/

#include "SessionDispatcher.h"
#include "CartaLib/LinearMap.h"
#include "core/MyQApp.h"
#include "core/SimpleRemoteVGView.h"
#include <iostream>
#include <QImage>
#include <QPainter>
#include <QXmlInputSource>
#include <cmath>
#include <QTime>
#include <QTimer>
#include <QCoreApplication>
#include <functional>

#include <QStringList>

#include <thread>
// #include "websocketclientwrapper.h"
// #include "websockettransport.h"
// #include "qwebchannel.h"
#include <QBuffer>
#include <QThread>

#include "NewServerConnector.h"
#include "CartaLib/Proto/register_viewer.pb.h"

void SessionDispatcher::startWebSocket(){

    int port = 3002;

    if (!m_hub.listen(port)){
        qFatal("Failed to open web socket server.");
        return;
    }

    qDebug() << "SessionDispatcher listening on port" << port;

    m_hub.onConnection([this](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req){
        onNewConnection(ws);
    });

    m_hub.onMessage([this](uWS::WebSocket<uWS::SERVER> *ws, char* message, size_t length, uWS::OpCode opcode){
        if (opcode == uWS::OpCode::TEXT){
            onTextMessage(ws, message, length);
        }
        else if (opcode == uWS::OpCode::BINARY){
            onBinaryMessage(ws, message, length);
        }
    });

    // repeat calling non-blocking poll() in qt event loop
    loopPoll();
}

void SessionDispatcher::loopPoll(){
    m_hub.poll();
    // submit a queue into qt eventloop
    defer([this](){
        loopPoll();
    });
}

SessionDispatcher::SessionDispatcher()
{
}

SessionDispatcher::~SessionDispatcher()
{
}

void SessionDispatcher::onNewConnection(uWS::WebSocket<uWS::SERVER> *socket)
{
    qDebug() << "A new connection!!";
}

void SessionDispatcher::onTextMessage(uWS::WebSocket<uWS::SERVER> *ws, char* message, size_t length){
    NewServerConnector* connector= sessionList[ws];
    QString cmd = QString::fromStdString(std::string(message, length));
    if (connector != nullptr){
        emit connector->onTextMessageSignal(cmd);
    }
}

void SessionDispatcher::onBinaryMessage(uWS::WebSocket<uWS::SERVER> *ws, char* message, size_t length){

    if (length < EVENT_NAME_LENGTH + EVENT_ID_LENGTH) {
        qFatal("Illegal message.");
        return;
    }

    size_t nullIndex = 0;
    for (size_t i = 0; i < EVENT_NAME_LENGTH; i++) {
        if (!message[i]) {
            nullIndex = i;
            break;
        }
    }

    QString eventName = QString::fromStdString(std::string(message, nullIndex));
    qDebug() << "Event received: " << eventName << QTime::currentTime().toString();

    if ( eventName == "REGISTER_VIEWER" ){

        bool sessionExisting = false;
        // TODO: replace the temporary way to generate ID
        QString sessionID = QString::number(std::rand());
        NewServerConnector *connector = new NewServerConnector();

        CARTA::RegisterViewer registerViewer;
        registerViewer.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
        if (registerViewer.session_id() != ""){
            sessionID = QString::fromStdString(registerViewer.session_id());
            qDebug() << "[SessionDispatcher] Get Session ID from frontend:" <<sessionID;
            if ( getConnectorInMap(sessionID) ) {
                connector = static_cast<NewServerConnector*>(getConnectorInMap(sessionID));
                sessionExisting = true;
                qDebug() << "[SessionDispatcher] Session ID from frontend exists in backend:" <<sessionID;
            }
        }
        else {
            qDebug() << "[SessionDispatcher] Use Session ID generated by backend:" << sessionID;
            setConnectorInMap(sessionID, connector);
        }

        sessionList[ws] = connector;

        if ( !sessionExisting ){
            qRegisterMetaType < size_t > ( "size_t" );
            connect(connector, SIGNAL(startViewerSignal(const QString &)), connector, SLOT(startViewerSlot(const QString &)));
//            connect(connector, SIGNAL(onTextMessageSignal(QString)), connector, SLOT(onTextMessage(QString)));
            connect(connector, SIGNAL(onBinaryMessageSignal(char*, size_t)), connector, SLOT(onBinaryMessage(char*, size_t)));

//            connect(connector, SIGNAL(jsTextMessageResultSignal(QString)), this, SLOT(forwardTextMessageResult(QString)) );
            connect(connector, SIGNAL(jsBinaryMessageResultSignal(char*, size_t)), this, SLOT(forwardBinaryMessageResult(char*, size_t)) );

            // create a simple thread
            QThread* newThread = new QThread();

            // let the new thread handle its events
            connector->moveToThread(newThread);
            newThread->setObjectName(sessionID);

            // start new thread's event loop
            newThread->start();

            //trigger signal
            emit connector->startViewerSignal(sessionID);
        }

        // add the message
        std::shared_ptr<CARTA::RegisterViewerAck> ack(new CARTA::RegisterViewerAck());
        ack->set_session_id(sessionID.toStdString());

        if (connector) {
            ack->set_success(true);
        } else {
            ack->set_success(false);
        }

        if (sessionExisting) {
            ack->set_session_type(::CARTA::SessionType::RESUMED);
        } else {
            ack->set_session_type(::CARTA::SessionType::NEW);
        }

        // serialize the message and send to the frontend
        QString respName = "REGISTER_VIEWER_ACK";
        PBMSharedPtr msg = ack;
        bool success = false;
        size_t requiredSize = 0;
        std::vector<char> result = serializeToArray(message, respName, msg, success, requiredSize);
        if (success) {
            ws->send(result.data(), requiredSize, uWS::OpCode::BINARY);
            qDebug() << "Send event:" << respName << QTime::currentTime().toString();
        }

        return;
    }

    NewServerConnector* connector= sessionList[ws];
    if (!connector){
        qFatal("Cannot find the server connector");
        return;
    }

    if (connector != nullptr){
        emit connector->onBinaryMessageSignal(message, length);
    }
}

void SessionDispatcher::forwardTextMessageResult(QString result){
    uWS::WebSocket<uWS::SERVER> *ws = nullptr;
    NewServerConnector* connector = qobject_cast<NewServerConnector*>(sender());
    std::map<uWS::WebSocket<uWS::SERVER>*, NewServerConnector*>::iterator iter;
    for (iter = sessionList.begin(); iter != sessionList.end(); ++iter){
        if (iter->second == connector){
            ws = iter->first;
            break;
        }
    }
    if (ws){
        char *message = result.toUtf8().data();
        ws->send(message, result.toUtf8().length(), uWS::OpCode::TEXT);
    }
    else {
        qDebug() << "ERROR! Cannot find the corresponding websocket!";
    }
}

void SessionDispatcher::forwardBinaryMessageResult(char* message, size_t length){
    uWS::WebSocket<uWS::SERVER> *ws = nullptr;
    NewServerConnector* connector = qobject_cast<NewServerConnector*>(sender());
    std::map<uWS::WebSocket<uWS::SERVER>*, NewServerConnector*>::iterator iter;
    for (iter = sessionList.begin(); iter != sessionList.end(); ++iter){
        if (iter->second == connector){
            ws = iter->first;
            break;
        }
    }
    if (ws){
        ws->send(message, length, uWS::OpCode::BINARY);
    }
    else {
        qDebug() << "ERROR! Cannot find the corresponding websocket!";
    }
}

IConnector* SessionDispatcher::getConnectorInMap(const QString & sessionID){

    mutex.lock();
    auto iter = clientList.find(sessionID);

    if(iter != clientList.end()) {
        auto connector = iter->second;
        mutex.unlock();
        return connector;
    }

    mutex.unlock();
    return nullptr;
}

void SessionDispatcher::setConnectorInMap(const QString & sessionID, IConnector *connector){
    mutex.lock();
    clientList[sessionID] = connector;
    mutex.unlock();
}

// //TODO implement later
// void SessionDispatcher::jsSendKeepAlive(){
// //    qDebug() << "get keepalive packet !!!!";
// }

//********* will comment the below later

void SessionDispatcher::initialize(const InitializeCallback & cb)
{
}

void SessionDispatcher::setState(const QString& path, const QString & newValue)
{

}

QString SessionDispatcher::getState(const QString & path  )
{
    return "";
}

QString SessionDispatcher::getStateLocation( const QString& saveName ) const {
    return "";
}

IConnector::CallbackID SessionDispatcher::addCommandCallback(
        const QString & cmd,
        const IConnector::CommandCallback & cb)
{
    return 0;
}

IConnector::CallbackID SessionDispatcher::addMessageCallback(
        const QString & cmd,
        const IConnector::MessageCallback & cb)
{
    return 0;
}

IConnector::CallbackID SessionDispatcher::addStateCallback(
        IConnector::CSR path,
        const IConnector::StateChangedCallback & cb)
{
    return 0;
}

void SessionDispatcher::registerView(IView * view)
{

}

void SessionDispatcher::unregisterView( const QString& viewName ){
}

qint64 SessionDispatcher::refreshView(IView * view)
{
    return 0;
}

void SessionDispatcher::removeStateCallback(const IConnector::CallbackID & /*id*/)
{
    qFatal( "not implemented");
}

Carta::Lib::IRemoteVGView * SessionDispatcher::makeRemoteVGView(QString viewName)
{
    return nullptr;
}
//*********
