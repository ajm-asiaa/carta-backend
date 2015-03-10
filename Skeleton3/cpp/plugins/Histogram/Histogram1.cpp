#include "Histogram1.h"
#include "CartaLib/Hooks/Histogram.h"
#include "ImageHistogram.h"
#include "Globals.h"
#include "PluginManager.h"
#include "CartaLib/Hooks/LoadAstroImage.h"
#include <QDebug>


Histogram1::Histogram1( QObject * parent ) :
    QObject( parent ),
    m_histogram( nullptr),
    m_cartaImage( nullptr )
{
    }


Carta::Lib::Hooks::HistogramResult Histogram1::_computeHistogram(){
    
    vector<std::pair<double,double>> data;
    QString name;
    QString units = "";
    if ( m_histogram ){
        bool computed = m_histogram->compute();
        if ( computed ){
            data = m_histogram->getData();
            name = m_histogram->getName();
            units = m_histogram->getUnits();
        }
        else {
            qDebug() << "Could not generate histogram data";
        }
    }
    else {
        qDebug() << "Histogram not initialized";
    }
    Carta::Lib::Hooks::HistogramResult result(name, units, data);
    return result;
}

bool Histogram1::handleHook( BaseHook & hookData ){
    if ( hookData.is < Initialize > () ) {
        return true;
    }
    else if ( hookData.is < Carta::Lib::Hooks::HistogramHook > () ) {
        bool histSuccess = false;
        Carta::Lib::Hooks::HistogramHook & hook
        = static_cast < Carta::Lib::Hooks::HistogramHook & > ( hookData );

        std::vector<std::shared_ptr<Image::ImageInterface>> images = hook.paramsPtr->dataSource;
        if ( images.size() > 0 ){
            m_cartaImage = images.front();

            CCImageBase * ptr1 = dynamic_cast<CCImageBase*>( m_cartaImage.get());
            if( ! ptr1) {
                throw "not an image created by casaimageloader...";
            }

            if (m_cartaImage->pixelType() == Image::PixelType::Real32 ){

                casa::ImageInterface<casa::Float> * casaImage =
                        dynamic_cast<casa::ImageInterface<casa::Float> * > (ptr1-> getCasaImage());


                ImageHistogram<casa::Float>* hist = new ImageHistogram<casa::Float>();
                m_histogram.reset( hist );
                hist->setImage( casaImage );
            }
        }

        if ( m_histogram ){

            auto count = hook.paramsPtr->binCount;
            m_histogram->setBinCount( count );

            auto minChannel = hook.paramsPtr->minChannel;
            auto maxChannel = hook.paramsPtr->maxChannel;
            m_histogram->setChannelRange(minChannel, maxChannel);

            auto minIntensity = hook.paramsPtr->minIntensity;
            auto maxIntensity = hook.paramsPtr->maxIntensity;
            m_histogram->setIntensityRange(minIntensity, maxIntensity);

            hook.result = _computeHistogram();
            histSuccess = true;
        }
        else {
            hook.result = _computeHistogram();
            histSuccess = false;
        }
        return histSuccess;
    }
    qWarning() << "Sorry, histogram doesn't know how to handle this hook";
    return false;
}

std::vector < HookId > Histogram1::getInitialHookList(){
    return {
        Initialize::staticId,
        Carta::Lib::Hooks::HistogramHook::staticId
    };
}


Histogram1::~Histogram1(){

}


