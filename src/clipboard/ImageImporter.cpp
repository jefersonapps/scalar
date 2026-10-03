#include "ImageImporter.h"
#include <QImageReader>
#include <QBuffer>
#include <QFile>
#ifdef SCALAR_HAVE_WEBP
#include <webp/decode.h>
#endif
namespace scalar {
ImportedImage encodeImage(QImage image,Point center,PageSize page){
    if(image.isNull()||image.width()>8192||image.height()>8192||double(image.width())*image.height()>32000000)return {{}, {},"Imagem inválida ou muito grande (máximo 8192 px / 32 MP)."};
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
    if(!image.save(&buffer,"PNG")||bytes.size()>32*1024*1024)return {{}, {},"Não foi possível incorporar a imagem (máximo 32 MiB)."};
    double w=image.width()*25.4/96,h=image.height()*25.4/96;const double scale=std::min({1.,page.widthMm*0.65/w,page.heightMm*0.65/h});w*=scale;h*=scale;
    ImageObject object;object.id=newId();object.pixelWidth=image.width();object.pixelHeight=image.height();object.png=std::make_shared<const std::vector<std::uint8_t>>(bytes.begin(),bytes.end());object.corners={{center.x-w/2,center.y-h/2},{center.x+w/2,center.y-h/2},{center.x+w/2,center.y+h/2},{center.x-w/2,center.y+h/2}};
    return {std::move(object),std::move(image),{}};
}
ImportedImage importImageFile(const QString& path,Point center,PageSize page){
    QImageReader reader(path);reader.setAutoTransform(true);
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
