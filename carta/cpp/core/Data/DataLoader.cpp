#include <unistd.h>

#include <QDebug>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegExp>

#include "DataLoader.h"
#include "Util.h"
#include "Globals.h"
#include "IPlatform.h"
#include "State/UtilState.h"

#include <set>

namespace Carta {

namespace Data {

class DataLoader::Factory : public Carta::State::CartaObjectFactory {

public:

    Factory():
        CartaObjectFactory( "DataLoader" ){};

    Carta::State::CartaObject * create (const QString & path, const QString & id)
    {
        return new DataLoader (path, id);
    }
};

QString DataLoader::fakeRootDirName = "RootDirectory";
const QString DataLoader::CLASS_NAME = "DataLoader";
const QString DataLoader::DIR = "dir";
const QString DataLoader::CRTF = ".crtf";
const QString DataLoader::REG = ".reg";

bool DataLoader::m_registered =
        Carta::State::ObjectManager::objectManager()->registerClass ( CLASS_NAME,
                                                   new DataLoader::Factory());

DataLoader::DataLoader( const QString& path, const QString& id ):
    CartaObject( CLASS_NAME, path, id ){
}

QString DataLoader::getData(const QString& dirName, const QString& sessionId) {
    QString rootDirName = dirName;
    bool securityRestricted = isSecurityRestricted();
    //Just get the default if the user is trying for a directory elsewhere and
    //security is restricted.
    if ( securityRestricted && !dirName.startsWith( DataLoader::fakeRootDirName) ){
        rootDirName = "";
    }

    if ( rootDirName.length() == 0 || dirName == DataLoader::fakeRootDirName){
        if ( lastAccessedDir.length() == 0 ){
            lastAccessedDir = getRootDir(sessionId);
        }
        rootDirName = lastAccessedDir;
    }
    else {
        rootDirName = getFile( dirName, sessionId );
    }
    lastAccessedDir = rootDirName;
    QDir rootDir(rootDirName);
    QJsonObject rootObj;

    _processDirectory(rootDir, rootObj);

    if ( securityRestricted ){
        QString baseName = getRootDir( sessionId );
        QString displayName = rootDirName.replace( baseName, DataLoader::fakeRootDirName);
        rootObj.insert(Util::NAME, displayName);
    }

    QJsonDocument document(rootObj);
    QByteArray textArray = document.toJson();
    QString jsonText(textArray);
    return jsonText;
}

QString DataLoader::getFile( const QString& bogusPath, const QString& sessionId ) const {
    QString path( bogusPath );
    QString fakePath( DataLoader::fakeRootDirName );
    if( path.startsWith( fakePath )){
        QString rootDir = getRootDir( sessionId );
        QString baseRemoved = path.remove( 0, fakePath.length() );
        path = QString( "%1%2").arg( rootDir).arg( baseRemoved);
    }
    return path;
}

QString DataLoader::getRootDir(const QString& /*sessionId*/) const {
    return Globals::instance()-> platform()-> getCARTADirectory().append("Images");
}

QString DataLoader::getShortName( const QString& longName ) const {
    QString rootDir = getRootDir( "" );
    int rootLength = rootDir.length();
    QString shortName;
    if ( longName.contains( rootDir)){
        shortName = longName.right( longName.size() - rootLength - 1);
    }
    else {
        int lastSlashIndex = longName.lastIndexOf( QDir::separator() );
        if ( lastSlashIndex >= 0 ){
            shortName = longName.right( longName.size() - lastSlashIndex - 1);
        }
    }
    return shortName;
}

QStringList DataLoader::getShortNames( const QStringList& longNames ) const {
    QStringList shortNames;
    for ( int i = 0; i < longNames.size(); i++ ){
        QString shortName = getShortName( longNames[i] );
        shortNames.append( shortName );
    }
    return shortNames;
}

QString DataLoader::getLongName( const QString& shortName, const QString& sessionId ) const {
    QString longName = shortName;
    QString potentialLongName = getRootDir( sessionId) + QDir::separator() + shortName;
    QFile file( potentialLongName );
    if ( file.exists() ){
        longName = potentialLongName;
    }
    return longName;
}

// QString DataLoader::getFileList(const QString & params){

//     std::set<QString> keys = { "path" };
//     std::map<QString,QString> dataValues = Carta::State::UtilState::parseParamMap( params, keys );
//     QString dir = dataValues[*keys.begin()];
//     QString xml = getData( dir, "1" );
//     return xml;
// }

DataLoader::PBMSharedPtr DataLoader::getFileList( CARTA::FileListRequest fileListRequest ){
    std::string dir = fileListRequest.directory();
    std::shared_ptr<CARTA::FileListResponse> fileListResponse(new CARTA::FileListResponse());

    QString dirName = QString::fromStdString(dir);
    QString rootDirName = dirName;
    bool securityRestricted = isSecurityRestricted();
    //Just get the default if the user is trying for a directory elsewhere and
    //security is restricted.
    if ( securityRestricted && !dirName.startsWith( DataLoader::fakeRootDirName) ){
        rootDirName = "";
    }

    if ( rootDirName.length() == 0 || dirName == DataLoader::fakeRootDirName){
        if ( lastAccessedDir.length() == 0 ){
            lastAccessedDir = getRootDir("1");
        }
        rootDirName = lastAccessedDir;
    }
    else {
        rootDirName = getFile( dirName, "1" );
    }
    lastAccessedDir = rootDirName;
    fileListResponse->set_success(true);
    fileListResponse->set_directory(rootDirName.toStdString());

    QDir rootDir(rootDirName);

    if (!rootDir.exists()) {
        QString errorMsg = "Please check that "+rootDir.absolutePath()+" is a valid directory.";
        Util::commandPostProcess( errorMsg );
        return nullptr;
    }

    QString lastPart = rootDir.absolutePath();

    QDirIterator dit(rootDir.absolutePath(), QDir::NoFilter);
    while (dit.hasNext()) {
        dit.next();
        // skip "." and ".." entries
        if (dit.fileName() == "." || dit.fileName() == "..") {
            continue;
        }

        QString fileName = dit.fileInfo().fileName();
        if (dit.fileInfo().isDir()) {
            QString rootDirPath = rootDir.absolutePath();
            QString subDirPath = rootDirPath.append("/").append(fileName);

            if (_checkSubDir(subDirPath) == "image") {
                uint64_t fileSize = _subDirSize(subDirPath);
                CARTA::FileInfo *fileInfo = fileListResponse->add_files();
                fileInfo->set_type(CARTA::FileType::CASA);
                fileInfo->set_name(fileName.toStdString());
                fileInfo->set_size(fileSize);
                fileInfo->add_hdu_list();
            }
            else if (_checkSubDir(subDirPath) == "miriad") {
                uint64_t fileSize = _subDirSize(subDirPath);
                CARTA::FileInfo *fileInfo = fileListResponse->add_files();
                fileInfo->set_type(CARTA::FileType::MIRIAD);
                fileInfo->set_name(fileName.toStdString());
                fileInfo->set_size(fileSize);
                fileInfo->add_hdu_list();
            }
            else {
                fileListResponse->add_subdirectories(fileName.toStdString());
            }
        }
        else if (dit.fileInfo().isFile()) {
            QFile file(lastPart+QDir::separator()+fileName);
            if (file.open(QFile::ReadOnly)) {
                QString dataInfo = file.read(160);
                if (dataInfo.contains(QRegExp("^SIMPLE *= *T.* BITPIX*")) && !dataInfo.contains(QRegExp("\n"))) {
                    uint64_t fileSize = file.size();
                    CARTA::FileInfo *fileInfo = fileListResponse->add_files();
                    fileInfo->set_name(fileName.toStdString());
                    fileInfo->set_type(CARTA::FileType::FITS);
                    fileInfo->set_size(fileSize);
                    fileInfo->add_hdu_list();
                }
                file.close();
            }
        }
    }

    return fileListResponse;
}

DataLoader::PBMSharedPtr DataLoader::getFileInfo(CARTA::FileInfoRequest fileInfoRequest) {

    QString fileDir = QString::fromStdString(fileInfoRequest.directory());
    if (!QDir(fileDir).exists()) {
        qWarning() << "[File Info] File directory doesn't exist! (" << fileDir << ")";
        return nullptr;
    }

    QString fileName = QString::fromStdString(fileInfoRequest.file());
    QString fileFullName = fileDir + "/" + fileName;

    QString file = fileFullName.trimmed();
    auto res = Globals::instance()->pluginManager()->prepare<Carta::Lib::Hooks::LoadAstroImage>(file).first();
    std::shared_ptr<Carta::Lib::Image::ImageInterface> image;
    if (!res.isNull()) {
        image = res.val();
    } else {
        qWarning() << "[File Info] Can not open the image file! (" << file << ")";
        return nullptr;
    }

    // FileInfo: set name & type
    CARTA::FileInfo* fileInfo = new CARTA::FileInfo();
    fileInfo->set_name(fileInfoRequest.file());
    if (image->getType() == "FITSImage") {
        fileInfo->set_type(CARTA::FileType::FITS);
    } else {
        fileInfo->set_type(CARTA::FileType::CASA);
    }

    // FileInfoExtended init: set dimensions, width, height
    const std::vector<int> dims = image->dims();
    CARTA::FileInfoExtended* fileInfoExt = new CARTA::FileInfoExtended();
    fileInfoExt->set_dimensions(dims.size());
    fileInfoExt->set_width(dims[0]);
    fileInfoExt->set_height(dims[1]);

    // it may be not really the spectral axis, but we can regardless of it so far.
    if (dims.size() >= 3) {
        fileInfoExt->set_depth(dims[2]);
    }

    // it may be not really the stoke axis, but we can regardless of it so far.
    if (dims.size() >= 4) {
        fileInfoExt->set_stokes(dims[3]);
    }

    // FileInfoExtended part 1: add statistic information to fileInfoExt using ImageStats plugin
    if (false == _getStatisticInfo(fileInfoExt, image)) {
        qDebug() << "[File Info] Get statistic informtion error.";
    }

    // FileInfoExtended part 2: generate some customized information to fileInfoExt
    if (false == _genCustomizedInfo(fileInfoExt, image)) {
        qDebug() << "[File Info] Generate file information error.";
    }

    // FileInfoExtended part 3: add all fits headers to fileInfoExt
    if (false == getFitsHeaders(fileInfoExt, image)) {
        qDebug() << "[File Info] Get fits headers error!";
    }

    // FileInfoResponse
    std::shared_ptr<CARTA::FileInfoResponse> fileInfoResponse(new CARTA::FileInfoResponse());
    fileInfoResponse->set_success(true);
    fileInfoResponse->set_allocated_file_info(fileInfo);
    fileInfoResponse->set_allocated_file_info_extended(fileInfoExt);

    return fileInfoResponse;
}

// Get all fits headers and insert to header entry
bool DataLoader::getFitsHeaders(CARTA::FileInfoExtended* fileInfoExt,
                                 const std::shared_ptr<Carta::Lib::Image::ImageInterface> image) {
    // validate parameters
    if (nullptr == fileInfoExt || nullptr == image) {
        return false;
    }

    // get fits header map using FitsHeaderExtractor
    FitsHeaderExtractor fhExtractor;
    fhExtractor.setInput(image);
    std::map<QString, QString> headerMap = fhExtractor.getHeaderMap();
    
    // traverse whole map to return all entries for frontend to render (AST)
    for (auto iter = headerMap.begin(); iter != headerMap.end(); iter++) {
        if (false == _insertHeaderEntry(fileInfoExt, iter->first, iter->second)) {
            qDebug() << "Insert (" << iter->first << ", " << iter->second << ") to header entry error.";
            return false;
        }
    }

    return true;
}

// Get statistic informtion using ImageStats plugin
bool DataLoader::_getStatisticInfo(CARTA::FileInfoExtended* fileInfoExt,
                                 const std::shared_ptr<Carta::Lib::Image::ImageInterface> image) {
    // validate parameters
    if (nullptr == fileInfoExt || nullptr == image) {
        return false;
    }

    // the statistical plugin requires a vector of ImageInterface
    std::vector<std::shared_ptr<Carta::Lib::Image::ImageInterface>> images;
    images.push_back(image);
    
    // regions is an empty setting so far
    std::vector<std::shared_ptr<Carta::Lib::Regions::RegionBase>> regions;
    
    // get the statistical data of the whole image
    std::vector<int> frameIndices(image->dims().size(), -1);

    if (images.size() > 0) { // [TODO]: do we really need this if statement?
        // Prepare to use the ImageStats plugin.
        auto result = Globals::instance()->pluginManager()
                -> prepare <Carta::Lib::Hooks::ImageStatisticsHook>(images, regions, frameIndices);

        // lamda function for traverse
        auto lam = [=] (const Carta::Lib::Hooks::ImageStatisticsHook::ResultType &data) {
            //An array for each image
            for (int i = 0; i < data.size(); i++) {
                // Each element of the image array contains an array of statistics.
                // Go through each set of statistics for the image.
                for (int j = 0; j < data[i].size(); j++) {
                    for (int k = 0; k < data[i][j].size(); k++) {
                        QString label = "- " + data[i][j][k].getLabel();
                        QString value = data[i][j][k].getValue();
                        if (false == _insertHeaderEntry(fileInfoExt, label, value))
                            qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
                    }
                }
            }
        };

        try {
            result.forEach(lam);
        } catch (char*& error) {
            QString errorStr(error);
            qDebug() << "[File Info] There is an error message: " << errorStr;
            return false;
        }
    }

    return true;
}

// Generate customized file information for human readiblity by using some fits headers
bool DataLoader::_genCustomizedInfo(CARTA::FileInfoExtended* fileInfoExt,
                                 const std::shared_ptr<Carta::Lib::Image::ImageInterface> image) {
    // validate parameters
    if (nullptr == fileInfoExt || nullptr == image) {
        qDebug() << "nullptr of fileInfoExt, image.";
        return false;
    }

    // get fits header map using FitsHeaderExtractor
    FitsHeaderExtractor fhExtractor;
    fhExtractor.setInput(image);
    std::map<QString, QString> headerMap = fhExtractor.getHeaderMap();

    // 1. Generate customized stokes + channels info & insert to header entry
    if (false == _genStokesChannelsInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate stokes + channels info & insert to header entry failed.";
    }

    // 2. Generate customized pixel unit info & insert to header entry
    if (false == _genPixelUnitInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate pixel unit info & insert to header entry failed.";
    }

    // 3. Generate customized pixel size info & insert to header entry
    if (false == _genPixelSizeInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate pixel size info & insert to header entry failed.";
    }

    // 4. Generate customized coordinate type info & insert to header entry
    if (false == _genCoordTypeInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate coordinate type info & insert to header entry failed.";
    }

    // 5. Generate customized image reference coordinate info & insert to header entry
    if (false == _genImgRefCoordInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate image reference coordinate info & insert to header entry failed.";
    }

    // 6. Generate customized celestial frame info & insert to header entry
    if (false == _genCelestialFrameInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate celestial frame info & insert to header entry failed.";
    }

    // 7. Generate customized spectral frame info & insert to header entry
    if (false == _genSpectralFrameInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate spectral frame info & insert to header entry failed.";
    }

    // 8. Generate customized velocity definition info & insert to header entry
    if (false == _genVelocityDefInfo(fileInfoExt, headerMap)) {
        qDebug() << "Generate velocity definition info & insert to header entry failed.";
    }

    return true;
}

// Generate customized stokes & channels info according to CTYPE3, CTYPE4, NAXIS3, NAXIS4
bool DataLoader::_genStokesChannelsInfo(CARTA::FileInfoExtended* fileInfoExt,
                                   const std::map<QString, QString> headerMap) {
    auto naxis = headerMap.find("NAXIS");
    if (naxis == headerMap.end()) {
        qDebug() << "Cannot find NAXIS.";
        return false;
    }

    bool ok = false;
    double n = (naxis->second).toDouble(&ok);
    if (ok && n > 2) {
        // find CTYPE3, CTYPE4
        auto ctype3 = headerMap.find("CTYPE3");
        auto ctype4 = headerMap.find("CTYPE4");
        QString stokes = "NA", channels = "NA";

        // check CTYPE3, CTYPE4 to determine stokes
        if (ctype3 != headerMap.end() && (ctype3->second).contains("STOKES", Qt::CaseInsensitive)) {
            auto naxis3 = headerMap.find("NAXIS3");
            if (naxis3 == headerMap.end()) {
                qDebug() << "Cannot find NAXIS3.";
                return false;
            }
            stokes = naxis3->second;
        } else if (ctype4 != headerMap.end() && (ctype4->second).contains("STOKES", Qt::CaseInsensitive)) {
            auto naxis4 = headerMap.find("NAXIS4");
            if (naxis4 == headerMap.end()) {
                qDebug() << "Cannot find NAXIS4.";
                return false;
            }
            stokes = naxis4->second;
        }

        // check CTYPE3, CTYPE4 to determine channels
        // [TODO]: regular expression should be case insensitive
        if (ctype3 != headerMap.end() && (ctype3->second).contains(QRegExp("VOPT|FREQ"))) {
            auto naxis3 = headerMap.find("NAXIS3");
            if (naxis3 == headerMap.end()) {
                qDebug() << "Cannot find NAXIS3.";
                return false;
            }
            channels = naxis3->second;
        } else if (ctype4 != headerMap.end() && (ctype4->second).contains(QRegExp("VOPT|FREQ"))) {
            auto naxis4 = headerMap.find("NAXIS4");
            if (naxis4 == headerMap.end()) {
                qDebug() << "Cannot find NAXIS4.";
                return false;
            }
            channels = naxis4->second;
        }

        // insert stokes, channels to header entry if they are not "NA"
        if ("NA" != stokes) {
            if (false == _insertHeaderEntry(fileInfoExt, "- Number of Stokes", stokes)) {
                qDebug() << "Insert (- Number of Stokes, " << stokes << ") to header entry error.";
                return false;
            }
        }
        if ("NA" != channels) {
            if (false == _insertHeaderEntry(fileInfoExt, "- Number of Channels", channels)) {
                qDebug() << "Insert (- Number of Channels, " << channels << ") to header entry error.";
                return false;
            }
        }
    }

    return true;
}

// Generate customized pixel unit info according to BUNIT
bool DataLoader::_genPixelUnitInfo(CARTA::FileInfoExtended* fileInfoExt,
                                   const std::map<QString, QString> headerMap) {
    auto bunit = headerMap.find("BUNIT");
    if (bunit == headerMap.end()) {
        qDebug() << "Cannot find BUNIT.";
        return false;
    }

    // insert (label, value) to header entry
    QString label = "- Pixel unit";
    QString value = bunit->second;
    if (false == _insertHeaderEntry(fileInfoExt, label, value)) {
        qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
        return false;
    }

    return true;
}

// Generate customized pixel size info according to CDELT1, CDELT2, CUNIT1
bool DataLoader::_genPixelSizeInfo(CARTA::FileInfoExtended* fileInfoExt,
                                   const std::map<QString, QString> headerMap) {
    QString label = "- Pixel size";
    QString value = "";

    auto cdelt1 = headerMap.find("CDELT1");
    auto cdelt2 = headerMap.find("CDELT2");
    if (cdelt1 != headerMap.end() && cdelt2 != headerMap.end()) {
        // get CDELT1, CDELT2 & convert them to double
        QString delstr1 = cdelt1->second;
        QString delstr2 = cdelt2->second;
        bool ok = false;
        double d1 = 0, d2 = 0;
        d1 = (delstr1).toDouble(&ok);
        if (!ok) {
            qDebug() << "Convert degree to double error.";
            return false;
        }
        d2 = (delstr2).toDouble(&ok);
        if (!ok) {
            qDebug() << "Convert degree to double error.";
            return false;
        }

        // get unit & convert CDELT1 CDELT2 to arcsec if unit is degree
        auto unit = headerMap.find("CUNIT1");
        if (unit == headerMap.end()) {
            qDebug() << "Cannot find CUNIT1.";
            return false;
        }

        if ((unit->second).contains("deg", Qt::CaseInsensitive)) {
            QString arcs1 = "", arcs2 = "";
            if(false == _deg2arcsec(delstr1, arcs1)) {
                qDebug() << "Convert CDELT1 to arcsec error.";
                return false;
            }
            if(false == _deg2arcsec(delstr2, arcs2)) {
                qDebug() << "Convert CDELT2 to arcsec error.";
                return false;
            }
            delstr1 = arcs1;
            delstr2 = arcs2;
        } else { // not degree
            delstr1 = delstr1 + " " + unit->second;
            delstr2 = delstr2 + " " + unit->second;
        }

        // check whether CDELT1 & CDELT2 are the same(squre)
        if (abs(d1) == abs(d2)) {
            value = (d1 > 0) ? delstr1 : delstr2;
        } else {
            value = delstr1 + ", " + delstr2;
        }
    }

    // insert (label, value) to header entry
    if (false == _insertHeaderEntry(fileInfoExt, label, value)) {
        qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
        return false;
    }

    return true;
}

// Generate customized coordinate type info according to CTYPE1, CTYPE2
bool DataLoader::_genCoordTypeInfo(CARTA::FileInfoExtended* fileInfoExt,
                                   const std::map<QString, QString> headerMap) {
    auto ctype1 = headerMap.find("CTYPE1");
    auto ctype2 = headerMap.find("CTYPE2");
    if (ctype1 == headerMap.end() || ctype2 == headerMap.end()) {
        qDebug() << "Cannot find CTYPE1 CTYPE2.";
        return false;
    }

    // insert (label, value) to header entry
    QString label = "- Coordinate type";
    QString value = ctype1->second + ", " + ctype2->second;
    if (false == _insertHeaderEntry(fileInfoExt, label, value)) {
        qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
        return false;
    }

    return true;
}

// Generate customized image reference coordinate info according to CRPIX1, CRPIX2, CRVAL1, CRVAL2, CUNIT1, CUNIT2
bool DataLoader::_genImgRefCoordInfo(CARTA::FileInfoExtended* fileInfoExt,
                                    const std::map<QString, QString> headerMap) {
    auto crpix1 = headerMap.find("CRPIX1");
    auto crpix2 = headerMap.find("CRPIX2");
    auto crval1 = headerMap.find("CRVAL1");
    auto crval2 = headerMap.find("CRVAL2");
    auto cunit1 = headerMap.find("CUNIT1");
    auto cunit2 = headerMap.find("CUNIT2");

    if (crpix1 == headerMap.end() || crpix2 == headerMap.end() ||
        crval1 == headerMap.end() || crval2 == headerMap.end() ||
        cunit1 == headerMap.end() || cunit2 == headerMap.end()) {
        qDebug() << "Cannot find CRPIX1 CRPIX2 CRVAL1 CRVAL2 CUNIT1 CUNIT2.";
        return false;
    }

     // insert (label, value) to header entry
    QString label = "- Image reference coordinate";
    QString value = "[" + crpix1->second + ", " + crpix2->second + "] [" +
                    crval1->second + " " + cunit1->second + ", " + crval2->second + " " + cunit2->second + "]";
    if (false == _insertHeaderEntry(fileInfoExt, label, value)) {
        qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
        return false;
    }

    return true;
}

// Generate customized celestial frame according to RADESYS, EQUINOX
bool DataLoader::_genCelestialFrameInfo(CARTA::FileInfoExtended* fileInfoExt,
                                        const std::map<QString, QString> headerMap) {
    auto radesys = headerMap.find("RADESYS");
    auto equinox = headerMap.find("EQUINOX");
    if (radesys == headerMap.end() || equinox == headerMap.end()) {
        qDebug() << "Cannot find RADESYS EQUINOX.";
        return false;
    }

    // get value of RADESYS, EQUINOX
    QString rad = radesys->second;
    QString equ = equinox->second;

    // FK4 => B1950, FK5 => J2000, others => not modified
    if (rad.contains("FK4", Qt::CaseInsensitive)) {
        bool ok = false;
        int e = equ.toDouble(&ok);
        if (!ok) {return false;}
        equ = "B" + QString(std::to_string(e).c_str());
    } else if (rad.contains("FK5", Qt::CaseInsensitive)) {
        bool ok = false;
        int e = equ.toDouble(&ok);
        if (!ok) {return false;}
        equ = "J" + QString(std::to_string(e).c_str());
    }

    // insert (label, value) to header entry
    QString label = "- Celestial frame";
    QString value = rad + ", " + equ;
    if (false == _insertHeaderEntry(fileInfoExt, label, value)) {
        qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
        return false;
    }

    return true;
}

// Generate customized spectral frame according to SPECSYS
bool DataLoader::_genSpectralFrameInfo(CARTA::FileInfoExtended* fileInfoExt,
                                        const std::map<QString, QString> headerMap) {
    auto specsys = headerMap.find("SPECSYS");
    if (specsys == headerMap.end()) {
        qDebug() << "Cannot find SPECSYS.";
        return false;
    }

    // insert (label, value) to header entry
    QString label = "- Spectral frame";
    QString value = specsys->second;
    if (false == _insertHeaderEntry(fileInfoExt, label, value)) {
        qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
        return false;
    }

    return true;
}

// Generate customized velocity definition according to VELREF
bool DataLoader::_genVelocityDefInfo(CARTA::FileInfoExtended* fileInfoExt,
                                        const std::map<QString, QString> headerMap) {
    auto velref = headerMap.find("VELREF");
    if (velref == headerMap.end()) {
        qDebug() << "Cannot find VELREF.";
        return false;
    }

    // insert (label, value) to header entry
    QString label = "- Velocity definition";
    QString value = velref->second;
    if (false == _insertHeaderEntry(fileInfoExt, label, value)) {
        qDebug() << "Insert (" << label << ", " << value << ") to header entry error.";
        return false;
    }

    return true;
}

//  Insert fits header to header entry, the fits structure is like:
//  NAXIS1 = 1024
//  CTYPE1 = 'RA---TAN'
//  CDELT1 = -9.722222222222E-07
//   ...etc
bool DataLoader::_insertHeaderEntry(CARTA::FileInfoExtended* fileInfoExt, const QString key, const QString value) {
    // validate parameters
    if (nullptr == fileInfoExt) {
        return false;
    }

    // insert (key, value) to header entry
    CARTA::HeaderEntry* headerEntry = fileInfoExt->add_header_entries();
    if (nullptr == headerEntry) {
        return false;
    }
    headerEntry->set_name(key.toLocal8Bit().constData());
    headerEntry->set_value(value.toLocal8Bit().constData());

    return true;
}

void DataLoader::_initCallbacks(){

    //Callback for returning a list of data files that can be loaded.
//    addCommandCallback( "getData", [=] (const QString & /*cmd*/,
//            const QString & params, const QString & sessionId) -> QString {
//        std::set<QString> keys = { "path" };
//        std::map<QString,QString> dataValues = Carta::State::UtilState::parseParamMap( params, keys );
//        QString dir = dataValues[*keys.begin()];
//        QString xml = getData( dir, sessionId );
//        return xml;
//    });

//    addCommandCallback( "isSecurityRestricted", [=] (const QString & /*cmd*/,
//                const QString & /*params*/, const QString & /*sessionId*/) -> QString {
//            bool securityRestricted = isSecurityRestricted();
//            QString result = "false";
//            if ( securityRestricted ){
//                result = true;
//            }
//            return result;
//        });
}

bool DataLoader::isSecurityRestricted() const {
    bool securityRestricted = Globals::instance()-> platform()-> isSecurityRestricted();
    return securityRestricted;
}

void DataLoader::_processDirectory(const QDir& rootDir, QJsonObject& rootObj) const {

    if (!rootDir.exists()) {
        QString errorMsg = "Please check that "+rootDir.absolutePath()+" is a valid directory.";
        Util::commandPostProcess( errorMsg );
        return;
    }

    QString lastPart = rootDir.absolutePath();
    rootObj.insert( Util::NAME, lastPart );

    QJsonArray dirArray;
    QDirIterator dit(rootDir.absolutePath(), QDir::NoFilter);
    while (dit.hasNext()) {
        dit.next();
        // skip "." and ".." entries
        if (dit.fileName() == "." || dit.fileName() == "..") {
            continue;
        }

        QString fileName = dit.fileInfo().fileName();
        if (dit.fileInfo().isDir()) {
            QString rootDirPath = rootDir.absolutePath();
            QString subDirPath = rootDirPath.append("/").append(fileName);

            if ( !_checkSubDir(subDirPath).isNull() ) {
                _makeFileNode( dirArray, fileName, _checkSubDir(subDirPath));
            }
            else {
                _makeFolderNode( dirArray, fileName );
            }
        }
        else if (dit.fileInfo().isFile()) {
            QFile file(lastPart+QDir::separator()+fileName);
            if (file.open(QFile::ReadOnly)) {
                QString dataInfo = file.read(160);
                if (dataInfo.contains("Region", Qt::CaseInsensitive)) {
                    if (dataInfo.contains("DS9", Qt::CaseInsensitive)) {
                        _makeFileNode(dirArray, fileName, "reg");
                    }
                    else if (dataInfo.contains("CRTF", Qt::CaseInsensitive)) {
                        _makeFileNode(dirArray, fileName, "crtf");
                    }
                }
                else if (dataInfo.contains(QRegExp("^SIMPLE *= *T.* BITPIX*")) && !dataInfo.contains(QRegExp("\n"))) {
                    _makeFileNode(dirArray, fileName, "fits");
                }
                file.close();
            }
        }
    }

    rootObj.insert( DIR, dirArray);
}

QString DataLoader::_checkSubDir( QString& subDirPath) const {

    QDir subDir(subDirPath);
    if (!subDir.exists()) {
        QString errorMsg = "Please check that "+subDir.absolutePath()+" is a valid directory.";
        Util::commandPostProcess( errorMsg );
        exit(0);
    }

    QMap< QString, QStringList> filterMap;

    QStringList imageFilters, miriadFilters;
    imageFilters << "table.f0_TSM0" << "table.info";
    miriadFilters << "header" << "image";

    filterMap.insert( "image", imageFilters);
    filterMap.insert( "miriad", miriadFilters);

    //look for the subfiles satisfying a special format
    foreach ( const QString &filter, filterMap.keys()){
        subDir.setNameFilters(filterMap.value(filter));
        if ( subDir.entryList().length() == filterMap.value(filter).length() ) {
            return filter;
        }
    }
    return NULL;
}

uint64_t DataLoader::_subDirSize(const QString &subDirPath) const {
    uint64_t totalSize = 0;
    QFileInfo str_info(subDirPath);
     if (str_info.isDir()) {
        QDir dir(subDirPath);
        dir.setFilter(QDir::Files | QDir::Dirs |  QDir::Hidden | QDir::NoSymLinks);
        QFileInfoList list = dir.entryInfoList();
         for (int i = 0; i < list.size(); i++) {
            QFileInfo fileInfo = list.at(i);
            if ((fileInfo.fileName() != ".") && (fileInfo.fileName() != "..")) {
                totalSize += (fileInfo.isDir()) ? this->_subDirSize(fileInfo.filePath()) : fileInfo.size();
            }
        }
    }
    return totalSize;
}

void DataLoader::_makeFileNode(QJsonArray& parentArray, const QString& fileName, const QString& fileType) const {
    QJsonObject obj;
    QJsonValue fileValue(fileName);
    obj.insert( Util::NAME, fileValue);
    //use type to represent the format of files
    //the meaning of "type" may differ with other codes
    //can change the string when feeling confused
    obj.insert( Util::TYPE, QJsonValue(fileType));
    parentArray.append(obj);
}

void DataLoader::_makeFolderNode( QJsonArray& parentArray, const QString& fileName ) const {
    QJsonObject obj;
    QJsonValue fileValue(fileName);
    obj.insert( Util::NAME, fileValue);
    QJsonArray arry;
    obj.insert(DIR, arry);
    parentArray.append(obj);
}

// Unit conversion: convert degree to arcsec
bool DataLoader::_deg2arcsec(const QString degree, QString& arcsec) {
    // convert degree to double
    bool ok = false;
    double deg = degree.toDouble(&ok);

    if(!ok) {
        qDebug() << "Convert degree to double error.";
        return false;
    }

    // convert degree to arcsec
    double arcs = deg * 3600;

    // customized format of arcsec
    char buf[512];
    if (arcs >= 60.0){ // arcs >= 60, convert to arcmin
        snprintf(buf, sizeof(buf), "%.2f\'", arcs/60);
    } else if (arcs < 60.0 && arcs > 0.1) { // 0.1 < arcs < 60
        snprintf(buf, sizeof(buf), "%.2f\"", arcs);
    } else if (arcs <= 0.1 && arcs > 0.01) { // 0.01 < arcs <= 0.1
        snprintf(buf, sizeof(buf), "%.3f\"", arcs);
    } else { // arcs <= 0.01
        snprintf(buf, sizeof(buf), "%.4f\"", arcs);
    }

    arcsec = QString(buf);
    return true;
}

DataLoader::~DataLoader(){
}
}
}
