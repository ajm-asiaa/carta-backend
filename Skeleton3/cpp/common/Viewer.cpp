#include "Viewer.h"
#include "Globals.h"
#include "IPlatform.h"
#include "IConnector.h"
#include "misc.h"
#include "PluginManager.h"
#include <iostream>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <cmath>
#include <QDebug>
#include <QCoreApplication>

//Globals & globals = * Globals::instance();

class TestView : public IView
{

public:

    TestView( const QString & viewName, QColor bgColor) {
        m_qimage = QImage( 100, 100, QImage::Format_RGB888);
        m_qimage.fill( bgColor);

        m_viewName = viewName;
        m_connector= nullptr;
        m_bgColor = bgColor;
    }

    virtual void registration(IConnector *connector)
    {
        m_connector = connector;
    }
    virtual const QString & name() const
    {
        return m_viewName;
    }
    virtual QSize size()
    {
        return m_qimage.size();
    }
    virtual const QImage & getBuffer()
    {
        redrawBuffer();
        return m_qimage;
    }
    virtual void handleResizeRequest(const QSize & size)
    {
        m_qimage = QImage( size, m_qimage.format());
        m_connector-> refreshView( this);
    }
    virtual void handleMouseEvent(const QMouseEvent & ev)
    {
        m_lastMouse = QPointF( ev.x(), ev.y());
        m_connector-> refreshView( this);

        m_connector-> setState( "/mouse/x", QString::number(ev.x()));
        m_connector-> setState( "/mouse/y", QString::number(ev.y()));
    }
    virtual void handleKeyEvent(const QKeyEvent & /*event*/)
    {
    }

protected:

    QColor m_bgColor;

    void redrawBuffer()
    {
        QPointF center = m_qimage.rect().center();
        QPointF diff = m_lastMouse - center;
        double angle = atan2( diff.x(), diff.y());
        angle *= - 180 / M_PI;

        m_qimage.fill( m_bgColor);
        {
            QPainter p( & m_qimage);
            p.setPen( Qt::NoPen);
            p.setBrush( QColor( 255, 255, 0, 128));
            p.drawEllipse( QPoint(m_lastMouse.x(), m_lastMouse.y()), 10, 10 );
            p.setPen( QColor( 255, 255, 255));
            p.drawLine( 0, m_lastMouse.y(), m_qimage.width()-1, m_lastMouse.y());
            p.drawLine( m_lastMouse.x(), 0, m_lastMouse.x(), m_qimage.height()-1);

            p.translate( m_qimage.rect().center());
            p.rotate( angle);
            p.translate( - m_qimage.rect().center());
            p.setFont( QFont( "Arial", 20));
            p.setPen( QColor( "white"));
            p.drawText( m_qimage.rect(), Qt::AlignCenter, m_viewName);
        }

        // execute the pre-render hook
        Globals::pluginManager()-> prepare<PreRender>( m_viewName, & m_qimage).executeAll();
    }

    IConnector * m_connector;
    QImage m_qimage;
    QString m_viewName;
    int m_timerId;
    QPointF m_lastMouse;
};

class TestView2 : public IView
{

public:

    TestView2( const QString & viewName, QColor bgColor, QImage img) {
        m_defaultImage = img;
        m_qimage = QImage( 100, 100, QImage::Format_RGB888);
        m_qimage.fill( bgColor);

        m_viewName = viewName;
        m_connector= nullptr;
        m_bgColor = bgColor;
    }

    virtual void registration(IConnector *connector)
    {
        m_connector = connector;
    }
    virtual const QString & name() const
    {
        return m_viewName;
    }
    virtual QSize size()
    {
        return m_qimage.size();
    }
    virtual const QImage & getBuffer()
    {
        redrawBuffer();
        return m_qimage;
    }
    virtual void handleResizeRequest(const QSize & size)
    {
        m_qimage = QImage( size, m_qimage.format());
        m_connector-> refreshView( this);
    }
    virtual void handleMouseEvent(const QMouseEvent & ev)
    {
        m_lastMouse = QPointF( ev.x(), ev.y());
        m_connector-> refreshView( this);

        m_connector-> setState( "/mouse/x", QString::number(ev.x()));
        m_connector-> setState( "/mouse/y", QString::number(ev.y()));
    }
    virtual void handleKeyEvent(const QKeyEvent & /*event*/)
    {
    }

protected:

    QColor m_bgColor;
    QImage m_defaultImage;

    void redrawBuffer()
    {
        QPointF center = m_qimage.rect().center();
        QPointF diff = m_lastMouse - center;
        double angle = atan2( diff.x(), diff.y());
        angle *= - 180 / M_PI;

        m_qimage.fill( m_bgColor);
        {
            QPainter p( & m_qimage);
            p.drawImage( m_qimage.rect(), m_defaultImage);
            p.setPen( Qt::NoPen);
            p.setBrush( QColor( 255, 255, 0, 128));
            p.drawEllipse( QPoint(m_lastMouse.x(), m_lastMouse.y()), 10, 10 );
            p.setPen( QColor( 255, 255, 255));
            p.drawLine( 0, m_lastMouse.y(), m_qimage.width()-1, m_lastMouse.y());
            p.drawLine( m_lastMouse.x(), 0, m_lastMouse.x(), m_qimage.height()-1);

            p.translate( m_qimage.rect().center());
            p.rotate( angle);
            p.translate( - m_qimage.rect().center());
            p.setFont( QFont( "Arial", 20));
            p.setPen( QColor( "white"));
            p.drawText( m_qimage.rect(), Qt::AlignCenter, m_viewName);
        }

        // execute the pre-render hook
        Globals::pluginManager()-> prepare<PreRender>( m_viewName, & m_qimage).executeAll();
    }

    IConnector * m_connector;
    QImage m_qimage;
    QString m_viewName;
    int m_timerId;
    QPointF m_lastMouse;
};

Viewer::Viewer( IPlatform * platform) :
    QObject( nullptr)
{
    Globals::setPlatform( platform);
    Globals::setConnector( platform->connector());
    Globals::setPluginManager( new PluginManager);

    auto pm = Globals::pluginManager();
    pm-> loadPlugins();
    // load plugins
    qDebug() << "Loading plugins...";
    auto infoList = pm-> getInfoList();
    qDebug() << "List of plugins: [" << infoList.size() << "]";
    for( const auto & entry : infoList) {
        qDebug() << "  path:" << entry-> path;
    }

    // tell all plugins that the core has initialized
    auto helper = pm-> prepare<Initialize>();
    helper.executeAll();
}

int Viewer::start()
{
    qDebug() << "Viewer::start() starting";

    // setup connector
    IConnector * connector = Globals::connector();

    if( ! connector || ! connector-> initialize()) {
        qCritical() << "Could not initialize connector.\n";
        exit( -1);
    }

    qDebug() << "Viewer::start: connector initialized\n";
    qDebug() << "Arguments:" << QCoreApplication::arguments();

#ifdef DONT_COMPILE

    // associate a callback for a command
    connector->addCommandCallback( "debug", [] (const QString & cmd, const QString & params, const QString & sessionId) -> QString {
        std::cerr << "lambda command cb:\n"
                  << " " << cmd << "\n"
                  << " " << params << "\n"
                  << " " << sessionId << "\n";
        return "1";
    });

    // associate a callback for a command
    connector->addCommandCallback( "add", [] (const QString & /*cmd*/, const QString & params, const QString & /*sessionId*/) -> QString {
        std::cerr << "add command:\n"
                  << params << "\n";
        QStringList lst = params.split(" ");
        double sum = 0;
        for( auto & entry : lst){
            bool ok;
            sum += entry.toDouble( & ok);
            if( ! ok) { sum = -1; break; }
        }
        return QString("add(%1)=%2").arg(params).arg(sum);
    });

//    auto xyzCBid =
    connector-> addStateCallback( "/xyz", [] ( const QString & path, const QString & val) {
        qDebug() << "lambda state cb:\n"
                 << "  path: " << path << "\n"
                 << "  val:  " << val;
    });
//    connector->removeStateCallback(xyzCBid);

    static const QString varPrefix = "/myVars";
    static int pongCount = 0;
    connector-> addStateCallback(
                varPrefix + "/ping",
                [=] ( const QString & path, const QString & val) {
        std::cerr << "lcb: " << path << "=" << val << "\n";
        QString nv = QString::number( pongCount ++);
        connector-> setState( varPrefix + "/pong", nv);
    });
    connector-> setState( "/xya", "hola");
    connector-> setState( "/xyz", "8");

#endif // dont compile

    // create some views to be rendered on the client side
    connector-> registerView( new TestView( "view1", QColor( "blue")));
    connector-> registerView( new TestView( "view2", QColor( "red")));

    qDebug() << "Arguments:" << QCoreApplication::arguments();


    // ask one of the plugins to load the image
    qDebug() << "======== trying to load image ========";
//    QString fname = Globals::fname();
    if( Globals::platform()-> initialFileList().isEmpty()) {
        qFatal( "No input given");
    }
    QString fname = Globals::platform()-> initialFileList() [0];
//    // this is a hack for server (since we don't have a way to pass encoded parameters yet)
//    // TODO: pass parameters from server
//    if( fname.isEmpty()) {
//        fname = "/scratch/testimage";
//    }

    auto loadImageHookHelper = Globals::pluginManager()-> prepare<LoadImage>( fname);
    Nullable<QImage> res = loadImageHookHelper.first();
    if( res.isNull()) {
        qDebug() << "Could not find any plugin to load image";
    }
    else {
        qDebug() << "Image loaded: " << res.val().size();
        connector-> registerView( new TestView2( "view3", QColor( "pink"), res.val()));
    }

    // give up control to Qt's main loop
//    qWarning() << "Giving up control to Qt...";
//    qDebug() << "Arguments:" << QCoreApplication::arguments();
//    qWarning() << "Giving up control to Qt...";

//    auto exitCode = QCoreApplication::instance()->exec();

//    return exitCode;

    return 0;
}


