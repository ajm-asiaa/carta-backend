/**
 *
 **/

#include "CartaLib/CartaLib.h"
#include <QString>

#pragma once

namespace Carta
{
namespace Lib
{
namespace Regions
{
typedef std::vector < double > PointN;

/// description of an axis in an image
class IAxisInfo
{
public:

    /// axis types we recognize
    enum class KnownType
    {
        DIRECTION_LON, /// < direction, longitude-ish
        DIRECTION_LAT, /// < direction, latitude-ish
        SPECTRAL,      /// < spectral axis
        STOKES,        /// < stokes axis
        PIXEL,         /// < pixel axis with no other information
        OTHER
    };

    /// return the type of this axis
    virtual KnownType
    knownType() const = 0;
};

class PixelAxisInfo : public IAxisInfo
{
public:

    virtual KnownType
    knownType() const override
    {
        return KnownType::PIXEL;
    }
};

class OtherAxisInfo : public IAxisInfo
{
public:

    OtherAxisInfo( QString otherInfo )
    {
        m_otherInfo = otherInfo;
    }

    virtual KnownType
    knownType() const override
    {
        return KnownType::OTHER;
    }

    QString
    otherInfo() const
    {
        return m_otherInfo;
    }

private:

    QString m_otherInfo;
};

class ICoordSystem
{
public:

    /// return a unique string (compact JSON)
    virtual QString
    serialized() = 0;

    /// compare to another coordinate system
    virtual bool
    isSameAs( ICoordSystem * otherCS ) = 0;

    /// return the world coordinate system (could be * this)
//    const ICoordSystem & worldCS();

    /// return the

    /// return information about axes
    virtual std::vector < const IAxisInfo * > &
    axisInfo() = 0;
};

/// default coordinate system - i.e. none
class DefaultCoordSystem : public ICoordSystem
{
public:

    DefaultCoordSystem( int nCoords = 2 )
    {
        CARTA_ASSERT( nCoords > 0 );
        m_axisInfos.resize( nCoords );
        for ( auto & ai : m_axisInfos ) {
            ai = new PixelAxisInfo();
        }
    }

    virtual QString
    serialized() override { return ""; }

    virtual bool
    isSameAs( ICoordSystem * otherCS ) override
    {
        return serialized() == otherCS->serialized();
    }

    virtual std::vector < IAxisInfo const * > &
    axisInfo() override
    {
        return m_axisInfos;
    }

private:

    std::vector < const IAxisInfo * > m_axisInfos;
};

/// coordinate system converter interface
class ICoordSystemConverter
{
public:

    virtual bool
    convert( const PointN & src, PointN & dst ) = 0;

    virtual const ICoordSystem &
    srcCS() = 0;

    virtual const ICoordSystem &
    dstCS() = 0;
};
}
}
}
