#include "ImageImporter.h"
#include <QImageReader>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryFile>
#include <QDir>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#ifdef SCALAR_HAVE_QT_SVG
#include <QSvgRenderer>
#endif
#ifdef SCALAR_HAVE_WEBP
#include <webp/decode.h>
#endif
namespace scalar {
QVariantList imageDropSources(const QVariantList& urls,const QString& html){
    static const QRegularExpression imageTag("<img\\b[^>]*>",QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression source("\\ssrc\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))",QRegularExpression::CaseInsensitiveOption);
    const auto tag=imageTag.match(html.left(1024*1024));
    if(tag.hasMatch()){
        const auto match=source.match(tag.captured());
        if(match.hasMatch()){
            const auto encoded=match.captured(1).isEmpty()?(match.captured(2).isEmpty()?match.captured(3):match.captured(2)):match.captured(1);
            const QUrl url(QTextDocumentFragment::fromHtml(encoded).toPlainText());
            if(url.isValid()&&(url.scheme()=="https"||url.scheme()=="http"||url.scheme()=="data"||url.isLocalFile()))return {url};
        }
    }
    return urls;
}
ImportedImage importImageUrl(const QUrl& url,Point center,PageSize page){
    if(url.isLocalFile())return importImageFile(url.toLocalFile(),center,page);
    constexpr qint64 limit=32*1024*1024;
    QByteArray bytes;
    if(url.scheme()=="data"){
        const auto encoded=url.toEncoded();const auto comma=encoded.indexOf(',');
        if(comma<0||encoded.size()>limit*3)return {{},{},"Imagem incorporada inválida ou muito grande."};
        const auto header=encoded.first(comma);
        if(!header.startsWith("data:image/"))return {{},{},"O conteúdo arrastado não é uma imagem."};
        const auto payload=QByteArray::fromPercentEncoding(encoded.mid(comma+1));
        bytes=header.endsWith(";base64")?QByteArray::fromBase64(payload):payload;
    }else if(url.scheme()=="http"||url.scheme()=="https"){
        QNetworkAccessManager network;QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(15000);
        QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);
        auto* reply=network.get(request);bool oversized=false,timedOut=false;
        const auto read=[&]{
            if(bytes.size()+reply->bytesAvailable()>limit){oversized=true;reply->abort();return;}
            bytes+=reply->readAll();
        };
        QObject::connect(reply,&QIODevice::readyRead,&loop,read);
        QObject::connect(reply,&QNetworkReply::metaDataChanged,&loop,[&]{
            if(reply->header(QNetworkRequest::ContentLengthHeader).toLongLong()>limit){oversized=true;reply->abort();}
        });
        QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
        QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timedOut=true;reply->abort();});
        deadline.start(20000);loop.exec();deadline.stop();
        if(oversized)return {{},{},"A imagem da web excede 32 MiB."};
        if(timedOut)return {{},{},"O site demorou demais para enviar a imagem."};
        if(reply->error()!=QNetworkReply::NoError)return {{},{},"Não foi possível baixar a imagem: "+reply->errorString()};
        read();
    }else return {{},{},"Arraste uma imagem com endereço HTTP, HTTPS ou um arquivo local."};
    if(bytes.isEmpty()||bytes.size()>limit)return {{},{},"Imagem vazia ou maior que 32 MiB."};
    // Reuse the same decoders and size limits as local files, including WEBP/SVG.
    const bool svg=bytes.left(1024).contains("<svg");
    QTemporaryFile file(QDir::tempPath()+ (svg?"/scalar-web-XXXXXX.svg":"/scalar-web-XXXXXX.img"));
    if(!file.open()||file.write(bytes)!=bytes.size()||!file.flush())return {{},{},"Não foi possível preparar a imagem baixada."};
    return importImageFile(file.fileName(),center,page);
}
ImportedImage encodeImage(QImage image,Point center,PageSize page){
    if(image.isNull()||image.width()>8192||image.height()>8192||double(image.width())*image.height()>32000000)return {{}, {},"Imagem inválida ou muito grande (máximo 8192 px / 32 MP)."};
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
    if(!image.save(&buffer,"PNG")||bytes.size()>32*1024*1024)return {{}, {},"Não foi possível incorporar a imagem (máximo 32 MiB)."};
    double w=image.width()*25.4/96,h=image.height()*25.4/96;const double scale=std::min({1.,page.widthMm*0.65/w,page.heightMm*0.65/h});w*=scale;h*=scale;
    ImageObject object;object.id=newId();object.pixelWidth=image.width();object.pixelHeight=image.height();object.png=std::make_shared<const std::vector<std::uint8_t>>(bytes.begin(),bytes.end());object.corners={{center.x-w/2,center.y-h/2},{center.x+w/2,center.y-h/2},{center.x+w/2,center.y+h/2},{center.x-w/2,center.y+h/2}};
    return {std::move(object),std::move(image),{}};
}
ImportedImage importImageFile(const QString& path,Point center,PageSize page){
#ifdef SCALAR_HAVE_QT_SVG
    if(QFileInfo(path).suffix().compare("svg",Qt::CaseInsensitive)==0){
        QFile file(path);if(!file.open(QIODevice::ReadOnly))return {{},{},file.errorString()};
        if(file.size()>32*1024*1024)return {{},{},"SVG excede 32 MiB."};
        QSvgRenderer renderer(file.readAll());const auto size=renderer.defaultSize();
        if(!renderer.isValid()||size.isEmpty()||size.width()>8192||size.height()>8192||qint64(size.width())*size.height()>32000000)return {{},{},"SVG inválido ou muito grande."};
        QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);renderer.render(&painter);painter.end();return encodeImage(std::move(image),center,page);
    }
#endif
    QFile source(path);
    if(!source.open(QIODevice::ReadOnly))return {{},{},"Não foi possível abrir a imagem: "+source.errorString()};
    QImageReader reader(&source);reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);
    if(!reader.canRead()){
        QFile file(path);
        if(file.open(QIODevice::ReadOnly)){
            const auto header=file.peek(12);
            if(header.size()==12&&header.startsWith("RIFF")&&header.mid(8,4)=="WEBP"){
#ifdef SCALAR_HAVE_WEBP
                if(file.size()>32*1024*1024)return {{},{},"Arquivo WEBP muito grande (máximo 32 MiB)."};
                const auto bytes=file.readAll();int width=0,height=0;
                const auto* data=reinterpret_cast<const std::uint8_t*>(bytes.constData());
                if(!WebPGetInfo(data,std::size_t(bytes.size()),&width,&height))return {{},{},"Arquivo WEBP inválido ou corrompido."};
                if(width>8192||height>8192||double(width)*height>32000000)return {{},{},"Imagem muito grande (máximo 8192 px / 32 MP)."};
                QImage image(width,height,QImage::Format_RGBA8888);
                if(image.isNull()||!WebPDecodeRGBAInto(data,std::size_t(bytes.size()),image.bits(),
                    std::size_t(image.sizeInBytes()),image.bytesPerLine()))return {{},{},"Não foi possível decodificar o WEBP."};
                return encodeImage(std::move(image),center,page);
#else
                return {{},{},"Suporte WEBP indisponível: instale o plugin de imagens Qt ou compile com libwebp."};
#endif
            }
        }
        return {{},{},"Não foi possível ler a imagem: "+reader.errorString()};
    }
    const auto size=reader.size();
    if(size.isValid()&&(size.width()>8192||size.height()>8192||double(size.width())*size.height()>32000000))
        return {{},{},"Imagem muito grande (máximo 8192 px / 32 MP)."};
    const auto image=reader.read();
    if(image.isNull())return {{},{},reader.errorString()};
    return encodeImage(image,center,page);
}
}
