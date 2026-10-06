#include "PdfService.h"
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QRegularExpression>
#include <QSet>
#include <QPainter>
#ifdef SCALAR_HAVE_QT_PDF
#include <QPdfDocument>
#endif
namespace scalar {
bool pdfAvailable(){
#ifdef SCALAR_HAVE_QT_PDF
    return true;
#else
    return false;
#endif
}
QSize pdfRenderSize(PageSize page,double scale){
    scale=std::pow(2.,std::ceil(std::log2(std::clamp(scale,0.25,32.))));
    const double width=page.widthMm*scale,height=page.heightMm*scale;
    const double limit=std::min({1.,4096./std::max(width,height),std::sqrt(16000000./(width*height))});
    return {std::max(1,int(std::floor(width*limit))),std::max(1,int(std::floor(height*limit)))};
}
std::vector<int> pdfPageRange(const QString& range,int count,QString& error){
    error.clear();std::vector<int> result;QSet<int> seen;
    if(count<1||count>1000){error="O PDF deve ter de 1 a 1000 páginas.";return {};}
    if(range.trimmed().isEmpty()){for(int i=0;i<count;++i)result.push_back(i);return result;}
    const QRegularExpression pattern("^\\s*(\\d+)\\s*(?:-\\s*(\\d+)\\s*)?$");
    for(const auto& part:range.split(',')){
        const auto match=pattern.match(part);bool ok=false;
        const int first=match.captured(1).toInt(&ok),last=match.captured(2).isEmpty()?first:match.captured(2).toInt();
        if(!match.hasMatch()||!ok||first<1||last<first||last>count){error="Use páginas como 1, 3-5 dentro do documento.";return {};}
        for(int i=first-1;i<last;++i)if(!seen.contains(i)){seen.insert(i);result.push_back(i);}
    }return result;
}
PdfInfo inspectPdf(const QString& path){
    PdfInfo result;result.name=QFileInfo(path).completeBaseName();QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){result.error=file.errorString();return result;}
    if(file.size()>64*1024*1024){result.error="PDF excede 64 MiB.";return result;}
    const auto bytes=file.readAll();result.assetId=newId();result.data=std::make_shared<const std::vector<std::uint8_t>>(bytes.begin(),bytes.end());
#ifdef SCALAR_HAVE_QT_PDF
    QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QPdfDocument document(nullptr);document.load(&buffer);
    if(document.status()!=QPdfDocument::Status::Ready){result.error="PDF inválido, corrompido ou protegido por senha.";return result;}
    if(document.pageCount()<1||document.pageCount()>1000){result.error="O PDF deve ter de 1 a 1000 páginas.";return result;}
    for(int i=0;i<document.pageCount();++i){const auto size=document.pagePointSize(i);PageSize physical{size.width()*25.4/72,size.height()*25.4/72};
        if(!physical.valid()){result.error="Uma página do PDF tem dimensões fora dos limites de 10 a 5000 mm.";result.sizes.clear();return result;}
        result.sizes.push_back(physical);
    }
#else
    result.error="Este build não inclui o módulo Qt PDF.";
#endif
    return result;
}
QImage renderPdf(const PdfPageObject& page,QSize size,QString* error){
    if(error)error->clear();
#ifdef SCALAR_HAVE_QT_PDF
    if(!page.data||page.data->empty()||size.isEmpty()||size.width()>4096||size.height()>4096||qint64(size.width())*size.height()>16000000){if(error)*error="Renderização PDF fora dos limites.";return {};}
    QBuffer buffer;buffer.setData(QByteArray(reinterpret_cast<const char*>(page.data->data()),qsizetype(page.data->size())));buffer.open(QIODevice::ReadOnly);
    QPdfDocument document(nullptr);document.load(&buffer);
    if(document.status()!=QPdfDocument::Status::Ready||page.pageIndex<0||page.pageIndex>=document.pageCount()){if(error)*error="Não foi possível abrir a página PDF incorporada.";return {};}
    const auto bitmap=document.render(page.pageIndex,size);
    if(bitmap.isNull()){if(error)*error="Não foi possível renderizar a página PDF.";return {};}
    // PDF paper is white even when the document has no explicit background.
    // Qt PDF returns transparent pixels there; flatten once in the cached render
    // so changing the board color cannot reveal hidden white text or lose black ink.
    QImage paper(bitmap.size(),QImage::Format_RGB32);paper.fill(Qt::white);
    QPainter painter(&paper);painter.drawImage(0,0,bitmap);painter.end();return paper;
#else
    if(error)*error="Este build não inclui o módulo Qt PDF.";return {};
#endif
}
}
