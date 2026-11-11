#include "ConstructionRecorder.h"
#include "rendering/PageRenderer.h"
#include "persistence/ProjectStore.h"
#include <QtConcurrent>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <cmath>
namespace scalar {
ConstructionRecorder::ConstructionRecorder(QObject* parent):QObject(parent){
    timer_.setInterval(800);
    connect(&timer_,&QTimer::timeout,this,[this]{if(position_+1>=count()){timer_.stop();emit changed();}else {const auto next=position_+1;seek(next);timer_.start();}});
    connect(&worker_,&QFutureWatcher<QString>::finished,this,[this]{
        captureBusy_=false;const auto failure=worker_.result();
        if(failure.isEmpty())frames_.append(directory_.filePath(QString("%1-%2.png").arg(generation_).arg(frames_.size())));
        else {error_=failure;recording_=false;queue_.clear();}
        process();emit changed();
    });
    connect(&exportWorker_,&QFutureWatcher<QString>::finished,this,[this]{error_=exportWorker_.result();exportStatus_=error_.isEmpty()?"Animação exportada":"";emit changed();});
}
ConstructionRecorder::~ConstructionRecorder(){worker_.waitForFinished();exportWorker_.waitForFinished();}
bool ConstructionRecorder::encoderAvailable() const{return !QStandardPaths::findExecutable("ffmpeg").isEmpty();}
void ConstructionRecorder::start(const Page& page,QRectF region){
    if(pending()||exporting()||!directory_.isValid()||!region.isValid()||!std::isfinite(region.width())||!std::isfinite(region.height()))return;
    clear();region_=region;pageId_=page.id;recording_=true;capture(page);
}
void ConstructionRecorder::capture(const Page& page){
    if(!recording_)return;
    if(page.id!=pageId_){stop();return;}
    queue_.push_back(page);process();emit changed();
}
void ConstructionRecorder::process(){
    if(captureBusy_||queue_.empty())return;
    captureBusy_=true;
    auto page=std::move(queue_.front());queue_.pop_front();
    const auto region=region_;const auto path=directory_.filePath(QString("%1-%2.png").arg(generation_).arg(frames_.size()));
    worker_.setFuture(QtConcurrent::run([page=std::move(page),region,path]{
        QImage image(1280,720,QImage::Format_RGB32);image.fill(QColor((page.background>>24)&255,(page.background>>16)&255,(page.background>>8)&255));
        QPainter painter(&image);const double scale=std::min(1280./region.width(),720./region.height());
        painter.translate((1280-region.width()*scale)/2,(720-region.height()*scale)/2);painter.scale(scale,scale);painter.translate(-region.x(),-region.y());
        const auto error=paintPage(painter,page,scale);painter.end();if(!error.isEmpty())return error;
        if(!image.save(path,"PNG"))return QString("Não foi possível gravar um passo da construção.");
        QSaveFile state(path+".json");const auto bytes=ProjectStore::serialize(Project{"recording","Construction","","",{page}});
        if(!state.open(QIODevice::WriteOnly)||state.write(bytes)!=bytes.size()||!state.commit())return QString("Não foi possível guardar os objetos do passo.");
        return QString{};
    }));
}
void ConstructionRecorder::stop(){recording_=false;timer_.stop();emit changed();}
void ConstructionRecorder::seek(int value){
    if(recording_||pending()||count()==0)return;
    timer_.stop();const auto target=std::clamp(value,0,count()-1);
    QFile file(frames_[target]+".json");if(!file.open(QIODevice::ReadOnly)){error_="Não foi possível abrir o passo.";emit changed();return;}
    const auto state=ProjectStore::deserialize(file.readAll());if(!state||state.project.pages.empty()){error_="O passo gravado está inválido.";emit changed();return;}
    replay_=state.project.pages.front();position_=target;emit changed();
}
void ConstructionRecorder::togglePlayback(){
    if(recording_||pending()||count()<2)return;
    if(playing())timer_.stop();else {if(position_<0||position_>=count()-1)seek(0);timer_.start();}emit changed();
}
void ConstructionRecorder::closePreview(){timer_.stop();position_=-1;replay_.reset();emit changed();}
void ConstructionRecorder::clear(){
    if(pending()||exporting())return;
    stop();for(const auto& path:frames_){QFile::remove(path);QFile::remove(path+".json");}replay_.reset();frames_.clear();position_=-1;error_.clear();exportStatus_.clear();++generation_;emit changed();
}
void ConstructionRecorder::deleteStep(int index){
    if(recording_||pending()||exporting()||index<0||index>=count())return;
    timer_.stop();const auto path=frames_.takeAt(index);QFile::remove(path);QFile::remove(path+".json");
    exportStatus_.clear();
    if(frames_.isEmpty())closePreview();else seek(std::min(index,count()-1));
}
void ConstructionRecorder::exportAnimation(const QUrl& destination,const QString& format){
    if(recording_||pending()||exporting()||count()<2)return;
    error_.clear();exportStatus_.clear();const auto encoder=QStandardPaths::findExecutable("ffmpeg");
    if(encoder.isEmpty()){error_="Instale o FFmpeg para exportar GIF ou MP4.";emit changed();return;}
    if(!destination.isLocalFile()||(format!="gif"&&format!="mp4")){error_="Escolha um arquivo GIF ou MP4 local.";emit changed();return;}
    auto target=destination.toLocalFile();if(!target.endsWith("."+format,Qt::CaseInsensitive))target+="."+format;
    const auto frames=frames_;
    const auto output=directory_.filePath("export."+format);
    const int frameCount=count();
    exportWorker_.setFuture(QtConcurrent::run([encoder,frames,output,target,format,frameCount]{
        QTemporaryDir sequence;if(!sequence.isValid())return QString("Não foi possível preparar a exportação.");
        for(int i=0;i<frames.size();++i)if(!QFile::copy(frames[i],sequence.filePath(QString::number(i)+".png")))return QString("Não foi possível preparar os passos da exportação.");
        const auto pattern=sequence.filePath("%d.png");
        QProcess process;QStringList args{"-y","-v","error","-framerate","5/4","-start_number","0","-i",pattern,"-frames:v",QString::number(frameCount)};
        if(format=="mp4")args<<"-c:v"<<"libx264"<<"-pix_fmt"<<"yuv420p"<<"-r"<<"30"<<"-movflags"<<"+faststart";
        else args<<"-filter_complex"<<"[0:v]split[a][b];[a]palettegen=stats_mode=single[p];[b][p]paletteuse=new=1"<<"-loop"<<"0";
        // The input frame count is enforced by the numbered sequence itself;
        // MP4 duplicates frames to retain the same duration as GIF playback.
        if(format=="mp4"){const auto index=args.indexOf("-frames:v");args.removeAt(index);args.removeAt(index);}
        args<<output;process.start(encoder,args);
        if(!process.waitForStarted(10000))return QString("Não foi possível iniciar o FFmpeg.");
        if(!process.waitForFinished(300000)){process.kill();process.waitForFinished();return QString("A exportação excedeu o tempo limite.");}
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)return QString("Falha na exportação: ")+QString::fromUtf8(process.readAllStandardError()).left(1000);
        QFile input(output);QSaveFile result(target);
        if(!input.open(QIODevice::ReadOnly)||!result.open(QIODevice::WriteOnly))return QString("Não foi possível salvar a animação no local escolhido.");
        while(!input.atEnd()){const auto bytes=input.read(1024*1024);if(bytes.isEmpty()&&input.error()!=QFile::NoError)return QString("Falha ao ler a animação exportada.");if(result.write(bytes)!=bytes.size())return QString("Falha ao gravar a animação.");}
        return result.commit()?QString{}:QString("Não foi possível concluir o arquivo exportado.");
    }));emit changed();
}
}
