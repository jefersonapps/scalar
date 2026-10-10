#include "PdfExporter.h"
#include "rendering/PageRenderer.h"
#include "geometry/Geometry.h"
#include <QPdfWriter>
#include <QPdfDocument>
#include <QPainter>
#include <QPageSize>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <cmath>
#ifdef SCALAR_HAVE_QT_SVG
#include <QSvgGenerator>
#endif
namespace scalar {
QString exportSelectedObjects(const std::vector<CanvasObject>& selected,const QString& path,bool svg){
    Page page;page.id=newId();page.background=0;page.size.infinite=true;
    for(const auto& object:selected)if(properties(object).visible)replaceObject(page,objectId(object),object);
    if(objects(page).empty())return "A seleção não possui objetos visíveis.";
    // Crop in physical units, with a tiny allowance for antialiasing.
    const auto area=pageRenderBounds(page,.5);
    page.size={std::max(1.,area.width()),std::max(1.,area.height())};
    const auto source=objects(page);for(const auto& object:source)replaceObject(page,objectId(object),transformed(object,{}, {-area.left,-area.top}));
    if(!svg){Project project;project.id=newId();project.name="Seleção Scalar";project.pages.push_back(std::move(page));return exportProjectPdf(project,path);}
#ifdef SCALAR_HAVE_QT_SVG
    QSaveFile output(path);output.setDirectWriteFallback(false);if(!output.open(QIODevice::WriteOnly))return output.errorString();
    QSvgGenerator generator;generator.setOutputDevice(&output);generator.setResolution(96);
    generator.setSize(QSize(std::max(1,int(std::ceil(page.size.widthMm*96/25.4))),std::max(1,int(std::ceil(page.size.heightMm*96/25.4)))));
    generator.setViewBox(QRectF(0,0,page.size.widthMm,page.size.heightMm));generator.setTitle("Seleção Scalar");
    QPainter painter(&generator);if(!painter.isActive())return "Não foi possível iniciar a exportação SVG.";
    const auto error=paintPage(painter,page,96/25.4);const bool ended=painter.end();if(!error.isEmpty())return error;
    if(!ended)return "Não foi possível finalizar o SVG.";
    if(!output.commit())return output.errorString();return {};
#else
    return "Esta instalação não possui suporte à exportação SVG.";
#endif
}
QString exportProjectPdf(const Project& project,const QString& path){
    if(project.pages.empty())return "O quadro não possui páginas.";
    std::vector<Bounds> areas;std::vector<PageSize> sizes;std::vector<double> factors;
    for(const auto& page:project.pages){
        if(!page.size.valid())return "Dimensões de página inválidas.";
        const auto area=pageRenderBounds(page);
        // Keep very large infinite drawings within PDF page-size limits while
        // preserving their proportions and including the entire drawing.
        const double factor=page.size.infinite?std::min({1.,5000./area.width(),5000./area.height()}):1.;
        areas.push_back(area);sizes.push_back({area.width()*factor,area.height()*factor});factors.push_back(factor);
    }
    QTemporaryFile temporary(QFileInfo(path).absolutePath()+"/.scalar-pdf-XXXXXX");
    if(!temporary.open())return temporary.errorString();
    {
        QPdfWriter writer(&temporary);writer.setResolution(300);writer.setTitle(QString::fromStdString(project.name));writer.setCreator("Scalar");
        const auto configure=[&](PageSize size){writer.setPageSize(QPageSize(QSizeF(size.widthMm,size.heightMm),QPageSize::Millimeter,{},QPageSize::ExactMatch));writer.setPageMargins(QMarginsF(0,0,0,0));};
        configure(sizes.front());QPainter painter(&writer);if(!painter.isActive())return "Não foi possível iniciar a exportação PDF.";
        for(std::size_t i=0;i<project.pages.size();++i){const auto& page=project.pages[i];
            if(!page.size.valid())return "Dimensões de página inválidas.";
            if(i){configure(sizes[i]);if(!writer.newPage())return "Não foi possível criar uma página PDF.";}
            painter.save();const double scale=writer.resolution()/25.4*factors[i];painter.scale(scale,scale);painter.translate(-areas[i].left,-areas[i].top);
            const auto error=paintPage(painter,page,scale);painter.restore();if(!error.isEmpty())return error;
        }
        if(!painter.end())return "Não foi possível finalizar o PDF.";
    }
    if(!temporary.flush())return temporary.errorString();
    {
        QPdfDocument verify(nullptr);if(verify.load(temporary.fileName())!=QPdfDocument::Error::None||verify.pageCount()!=int(project.pages.size()))return "Falha ao validar o PDF exportado.";
        for(int i=0;i<verify.pageCount();++i){const auto points=verify.pagePointSize(i);const auto size=sizes[i];
            if(std::abs(points.width()*25.4/72-size.widthMm)>0.5||std::abs(points.height()*25.4/72-size.heightMm)>0.5)return "O PDF não preservou as dimensões físicas da página.";
        }
    }
    if(!temporary.seek(0))return temporary.errorString();QSaveFile destination(path);destination.setDirectWriteFallback(false);
    if(!destination.open(QIODevice::WriteOnly))return destination.errorString();
    while(!temporary.atEnd()){const auto bytes=temporary.read(1024*1024);if(bytes.isEmpty()&&temporary.error()!=QFileDevice::NoError)return temporary.errorString();if(destination.write(bytes)!=bytes.size())return destination.errorString();}
    if(!destination.commit())return destination.errorString();return {};
}
}
