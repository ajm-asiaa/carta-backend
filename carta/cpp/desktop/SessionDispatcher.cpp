/**
 *
 **/

#include "SessionDispatcher.h"
#include "CartaLib/LinearMap.h"
#include "core/MyQApp.h"
#include <iostream>
#include <QXmlInputSource>
#include <cmath>
#include <QTime>
#include <QTimer>
#include <QCoreApplication>
#include <functional>

#include <QStringList>

#include <thread>
#include <QBuffer>
#include <QThread>
#include <QUuid>

#include "NewServerConnector.h"

#include "CartaLib/Proto/register_viewer.pb.h"
#include "CartaLib/Proto/set_image_channels.pb.h"
#include "CartaLib/Proto/set_image_view.pb.h"

#include "Globals.h"
#include "core/CmdLine.h"

void SessionDispatcher::startWebSocket(){

    int port = Globals::instance()->cmdLineInfo()-> port();

    if ( port < 0 ) {
        port = 3002;
        qDebug() << "Using default SessionDispatcher port" << port;
       }
    else {
        qDebug() << "SessionDispatcher listening on port" << port;
    }

    if (!m_hub.listen(port)){
        qCritical("Failed to open web socket server.");
        return;
    }

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
        QThread::msleep(10); // sleep for 10 ms in order to ease the loading of CPU
        loopPoll();
    });
}

SessionDispatcher::SessionDispatcher() {
}

SessionDispatcher::~SessionDispatcher() {
}

void SessionDispatcher::onNewConnection(uWS::WebSocket<uWS::SERVER> *socket) {
    qDebug() << "[SessionDispatcher] A new connection!!";
}

void SessionDispatcher::onTextMessage(uWS::WebSocket<uWS::SERVER> *ws, char* message, size_t length) {
    NewServerConnector* connector= sessionList[ws];
    QString cmd = QString::fromStdString(std::string(message, length));
    if (connector != nullptr) {
        emit connector->onTextMessageSignal(cmd);
    }
}

void SessionDispatcher::onBinaryMessage(uWS::WebSocket<uWS::SERVER> *ws, char* message, size_t length) {
    if (length < EVENT_NAME_LENGTH + EVENT_ID_LENGTH) {
        qDebug() << "message length=" << length << "is not valid!!";
        qWarning("[SessionDispatcher] Illegal message.");
        return;
    }

    // get the message Name
    size_t nullIndex = 0;
    for (size_t i = 0; i < EVENT_NAME_LENGTH; i++) {
        if (!message[i]) {
            nullIndex = i;
            break;
        }
    }
    QString eventName = QString::fromStdString(std::string(message, nullIndex));

    // get the message Id
    uint32_t eventId = *((uint32_t*) (message + EVENT_NAME_LENGTH));

    qDebug() << "[SessionDispatcher] Event received: Name=" << eventName << ", Id=" << eventId << ", length=" << length << ", Time=" << QTime::currentTime().toString();

    if (eventName == "REGISTER_VIEWER") {

        bool sessionExisting = false;
        QString sessionID = QUuid::createUuid().toString(); // generate a unique ID
        NewServerConnector *connector = new NewServerConnector();

        CARTA::RegisterViewer registerViewer;
        registerViewer.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);

        if (registerViewer.session_id() != "") {
            sessionID = QString::fromStdString(registerViewer.session_id());
            qDebug() << "[SessionDispatcher] Get Session ID from frontend:" << sessionID;
            if (getConnectorInMap(sessionID)) {
                connector = static_cast<NewServerConnector*>(getConnectorInMap(sessionID));
                sessionExisting = true;
                qDebug() << "[SessionDispatcher] Session ID from frontend exists in backend:" << sessionID;
            }
        } else {
            qDebug() << "[SessionDispatcher] Use Session ID generated by backend:" << sessionID;
            setConnectorInMap(sessionID, connector);
        }

        sessionList[ws] = connector;

        if (!sessionExisting) {
            qRegisterMetaType<size_t>("size_t");
            qRegisterMetaType<uint32_t>("uint32_t");
            qRegisterMetaType<PBMSharedPtr>("PBMSharedPtr");
            qRegisterMetaType<CARTA::Point>("CARTA::Point");
            qRegisterMetaType<CARTA::SetSpatialRequirements>("CARTA::SetSpatialRequirements");
            qRegisterMetaType<CARTA::FileListRequest>("CARTA::FileListRequest");
            qRegisterMetaType<CARTA::FileInfoRequest>("CARTA::FileInfoRequest");
            qRegisterMetaType<google::protobuf::RepeatedPtrField<std::string>>("google::protobuf::RepeatedPtrField<std::string>");
            qRegisterMetaType<google::protobuf::RepeatedPtrField<CARTA::SetSpectralRequirements_SpectralConfig>>("google::protobuf::RepeatedPtrField<CARTA::SetSpectralRequirements_SpectralConfig>");

            // start the image viewer
            connect(connector, SIGNAL(startViewerSignal(const QString &)),
                    connector, SLOT(startViewerSlot(const QString &)));

            // general commands
            //connect(connector, SIGNAL(onBinaryMessageSignal(const char*, size_t)),
            //        connector, SLOT(onBinaryMessageSignalSlot(const char*, size_t)));

            // file list request
            connect(connector, SIGNAL(fileListRequestSignal(uint32_t, CARTA::FileListRequest)),
                    connector, SLOT(fileListRequestSignalSlot(uint32_t, CARTA::FileListRequest)));

            // file info request
            connect(connector, SIGNAL(fileInfoRequestSignal(uint32_t, CARTA::FileInfoRequest)),
                    connector, SLOT(fileInfoRequestSignalSlot(uint32_t, CARTA::FileInfoRequest)));

            // open file
            connect(connector, SIGNAL(openFileSignal(uint32_t, QString, QString, int, int)),
                    connector, SLOT(openFileSignalSlot(uint32_t, QString, QString, int, int)));

            // set image view
            connect(connector, SIGNAL(setImageViewSignal(uint32_t, int , int, int, int, int, int, bool, int, int)),
                    connector, SLOT(setImageViewSignalSlot(uint32_t, int , int, int, int, int, int, bool, int, int)));

            // set image channel
            connect(connector, SIGNAL(imageChannelUpdateSignal(uint32_t, int, int, int)),
                    connector, SLOT(imageChannelUpdateSignalSlot(uint32_t, int, int, int)));

            // set cursor
            connect(connector, SIGNAL(setCursorSignal(uint32_t, int, CARTA::Point, CARTA::SetSpatialRequirements)),
                    connector, SLOT(setCursorSignalSlot(uint32_t, int, CARTA::Point, CARTA::SetSpatialRequirements)));

            // set spatial requirements
            connect(connector, SIGNAL(setSpatialRequirementsSignal(uint32_t, int, int, google::protobuf::RepeatedPtrField<std::string>)),
                    connector, SLOT(setSpatialRequirementsSignalSlot(uint32_t, int, int, google::protobuf::RepeatedPtrField<std::string>)));

            // set spectral requirements
            connect(connector, SIGNAL(setSpectralRequirementsSignal(uint32_t, int, int, google::protobuf::RepeatedPtrField<CARTA::SetSpectralRequirements_SpectralConfig>)),
                    connector, SLOT(setSpectralRequirementsSignalSlot(uint32_t, int, int, google::protobuf::RepeatedPtrField<CARTA::SetSpectralRequirements_SpectralConfig>)));

            // send binary signal to the frontend
            connect(connector, SIGNAL(jsBinaryMessageResultSignal(QString, uint32_t, PBMSharedPtr)),
                    this, SLOT(forwardBinaryMessageResult(QString, uint32_t, PBMSharedPtr)));

            //connect(connector, SIGNAL(onTextMessageSignal(QString)), connector, SLOT(onTextMessage(QString)));
            //connect(connector, SIGNAL(jsTextMessageResultSignal(QString)), this, SLOT(forwardTextMessageResult(QString)) );

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
        std::vector<char> result = serializeToArray(respName, eventId, msg, success, requiredSize);
        if (success) {
            ws->send(result.data(), requiredSize, uWS::OpCode::BINARY);
            qDebug() << "[SessionDispatcher] Send event:" << respName<< ", Id=" << eventId << ", length=" << requiredSize << QTime::currentTime().toString();
        }

        return;
    } // end of "REGISTER_VIEWER" block

    NewServerConnector* connector= sessionList[ws];

    if (!connector) {
        qCritical("[SessionDispatcher] Cannot find the server connector");
        return;
    }

    if (connector != nullptr) {
        if (eventName == "FILE_LIST_REQUEST") {

            CARTA::FileListRequest fileListRequest;
            fileListRequest.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            emit connector->fileListRequestSignal(eventId, fileListRequest);

        } else if (eventName == "FILE_INFO_REQUEST") {

            CARTA::FileInfoRequest fileInfoRequest;
            fileInfoRequest.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            emit connector->fileInfoRequestSignal(eventId, fileInfoRequest);

        } else if (eventName == "OPEN_FILE") {

            CARTA::OpenFile openFile;
            openFile.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            QString fileDir = QString::fromStdString(openFile.directory());
            QString fileName = QString::fromStdString(openFile.file());
            int fileId = openFile.file_id();
            // If the histograms correspond to the entire current 2D image, the region ID has a value of -1.
            int regionId = -1;
            qDebug() << "[SessionDispatcher] Open the image file" << fileDir + "/" + fileName << "(fileId=" << fileId << ")";
            emit connector->openFileSignal(eventId, fileDir, fileName, fileId, regionId);

        } else if (eventName == "SET_IMAGE_VIEW") {

            CARTA::SetImageView viewSetting;
            viewSetting.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            int fileId = viewSetting.file_id();
            int mip = viewSetting.mip();
            int xMin = viewSetting.image_bounds().x_min();
            int xMax = viewSetting.image_bounds().x_max();
            int yMin = viewSetting.image_bounds().y_min();
            int yMax = viewSetting.image_bounds().y_max();
            qDebug() << "[SessionDispatcher] Set image bounds [x_min, x_max, y_min, y_max, mip]=["
                     << xMin << "," << xMax << "," << yMin << "," << yMax << "," << mip << "], fileId=" << fileId;

            int numSubsets = viewSetting.num_subsets();
            int precision = lround(viewSetting.compression_quality());
            bool isZFP = (viewSetting.compression_type() == CARTA::CompressionType::ZFP) ? true : false;

            emit connector->setImageViewSignal(eventId, fileId, xMin, xMax, yMin, yMax, mip, isZFP, precision, numSubsets);

        } else if (eventName == "SET_IMAGE_CHANNELS") {

            CARTA::SetImageChannels setImageChannels;
            setImageChannels.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            int fileId = setImageChannels.file_id();
            int channel = setImageChannels.channel();
            int stokes = setImageChannels.stokes();
            qDebug() << "[SessionDispatcher] Set image channel=" << channel << ", fileId=" << fileId << ", stokes=" << stokes;
            emit connector->imageChannelUpdateSignal(eventId, fileId, channel, stokes);

        } else if (eventName == "SET_CURSOR") {

            CARTA::SetCursor setCursor;
            setCursor.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            int fileId = setCursor.file_id();
            CARTA::Point point = setCursor.point();
            CARTA::SetSpatialRequirements spatialReqs = setCursor.spatial_requirements();
            qDebug() << "[SessionDispatcher] Set cursor fileId=" << fileId << ", point=(" << point.x() << ", " << point.y() << ")";
            emit connector->setCursorSignal(eventId, fileId, point, spatialReqs);

        } else if (eventName == "SET_SPATIAL_REQUIREMENTS") {

            CARTA::SetSpatialRequirements setSpatialRequirements;
            setSpatialRequirements.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            int fileId = setSpatialRequirements.file_id();
            int regionId = setSpatialRequirements.region_id();
            google::protobuf::RepeatedPtrField<std::string> spatialProfiles = setSpatialRequirements.spatial_profiles();
            qDebug() << "[SessionDispatcher] Set Spatial Requirements fileId=" << fileId << ", regionId=" << regionId;
            for(auto iter = spatialProfiles.begin(); iter != spatialProfiles.end(); iter++) {
                qDebug() << "[SessionDispatcher] Spatial profile="  << QString::fromStdString(*iter);
            }
            emit connector->setSpatialRequirementsSignal(eventId, fileId, regionId, spatialProfiles);

        } else if (eventName == "SET_SPECTRAL_REQUIREMENTS") {

            CARTA::SetSpectralRequirements setSpectralRequirements;
            setSpectralRequirements.ParseFromArray(message + EVENT_NAME_LENGTH + EVENT_ID_LENGTH, length - EVENT_NAME_LENGTH - EVENT_ID_LENGTH);
            int fileId = setSpectralRequirements.file_id();
            int regionId = setSpectralRequirements.region_id();
            google::protobuf::RepeatedPtrField<CARTA::SetSpectralRequirements_SpectralConfig> spectralProfiles = setSpectralRequirements.spectral_profiles();
            qDebug() << "[SessionDispatcher] Set Spectral Requirements fileId=" << fileId << ", regionId=" << regionId;
            for(auto iter = spectralProfiles.begin(); iter != spectralProfiles.end(); iter++) {
                qDebug() << "[SessionDispatcher] Spectral profile coordinate="  << QString::fromStdString(iter->coordinate());
                auto stats_types = iter->stats_types();
                for(auto statIter = stats_types.begin(); statIter != stats_types.end(); statIter++) {
                    qDebug() << "[SessionDispatcher] Statistic types="  << QString(*statIter);
                }
            }
            emit connector->setSpectralRequirementsSignal(eventId, fileId, regionId, spectralProfiles);

        } else {
            qCritical() << "[SessionDispatcher] There is no event handler:" << eventName;
            //emit connector->onBinaryMessageSignal(message, length);
        }
    }
}

void SessionDispatcher::forwardTextMessageResult(QString result) {
    uWS::WebSocket<uWS::SERVER> *ws = nullptr;
    NewServerConnector* connector = qobject_cast<NewServerConnector*>(sender());
    std::map<uWS::WebSocket<uWS::SERVER>*, NewServerConnector*>::iterator iter;
    for (iter = sessionList.begin(); iter != sessionList.end(); ++iter) {
        if (iter->second == connector) {
            ws = iter->first;
            break;
        }
    }
    if (ws) {
        char *message = result.toUtf8().data();
        ws->send(message, result.toUtf8().length(), uWS::OpCode::TEXT);
    } else {
        qDebug() << "ERROR! Cannot find the corresponding websocket!";
    }
}

void SessionDispatcher::forwardBinaryMessageResult(QString respName, uint32_t eventId, PBMSharedPtr protoMsg) {
    uWS::WebSocket<uWS::SERVER> *ws = nullptr;
    NewServerConnector* connector = qobject_cast<NewServerConnector*>(sender());
    std::map<uWS::WebSocket<uWS::SERVER>*, NewServerConnector*>::iterator iter;
    for (iter = sessionList.begin(); iter != sessionList.end(); ++iter){
        if (iter->second == connector){
            ws = iter->first;
            break;
        }
    }
    if (ws) {
        // send serialized message to the frontend
        bool success = false;
        size_t requiredSize = 0;
        std::vector<char> message = serializeToArray(respName, eventId, protoMsg, success, requiredSize);
        if (success) {
            ws->send(message.data(), requiredSize, uWS::OpCode::BINARY);
            qDebug() << "[SessionDispatcher] Send event: Name=" << respName << ", Id=" << eventId << ", length=" << requiredSize << ", Time=" << QTime::currentTime().toString();
        }
    } else {
        qDebug() << "[SessionDispatcher] ERROR! Cannot find the corresponding websocket!";
    }
}

IConnector* SessionDispatcher::getConnectorInMap(const QString & sessionID) {
    mutex.lock();
    auto iter = clientList.find(sessionID);

    if (iter != clientList.end()) {
        auto connector = iter->second;
        mutex.unlock();
        return connector;
    }

    mutex.unlock();
    return nullptr;
}

void SessionDispatcher::setConnectorInMap(const QString & sessionID, IConnector *connector) {
    mutex.lock();
    clientList[sessionID] = connector;
    mutex.unlock();
}

// //TODO implement later
// void SessionDispatcher::jsSendKeepAlive(){
// //    qDebug() << "get keepalive packet !!!!";
// }

//********* will comment the below later

void SessionDispatcher::initialize(const InitializeCallback & cb) {

}

void SessionDispatcher::setState(const QString& path, const QString & newValue) {

}

QString SessionDispatcher::getState(const QString & path) {
    return "";
}

QString SessionDispatcher::getStateLocation(const QString& saveName) const {
    return "";
}

IConnector::CallbackID SessionDispatcher::addCommandCallback (
        const QString & cmd,
        const IConnector::CommandCallback & cb) {
    return 0;
}

IConnector::CallbackID SessionDispatcher::addMessageCallback (
        const QString & cmd,
        const IConnector::MessageCallback & cb) {
    return 0;
}

IConnector::CallbackID SessionDispatcher::addStateCallback (
        IConnector::CSR path,
        const IConnector::StateChangedCallback & cb) {
    return 0;
}

void SessionDispatcher::registerView(IView * view) {

}

void SessionDispatcher::unregisterView(const QString& viewName) {

}

qint64 SessionDispatcher::refreshView(IView * view) {
    return 0;
}

void SessionDispatcher::removeStateCallback(const IConnector::CallbackID & /*id*/) {
    qCritical( "not implemented");
}

Carta::Lib::IRemoteVGView * SessionDispatcher::makeRemoteVGView(QString viewName) {
    return nullptr;
}
