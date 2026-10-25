#include "MathRenderer.h"
#include "SvgValidation.h"
#include <QCoreApplication>
#include <QCache>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <memory>
namespace scalar {
namespace {
// All process access runs on one thread, including calls from QtConcurrent workers.
class MathRuntime {
    QThread thread_;
    QObject* context_=new QObject;
    std::unique_ptr<QProcess> process_;
    QString directory_,node_;
    QCache<QByteArray,QJsonArray> cache_{4*1024*1024};
    void stop(){if(process_){process_->kill();process_->waitForFinished(3000);process_.reset();}}
public:
    MathRuntime(){context_->moveToThread(&thread_);QObject::connect(&thread_,&QThread::finished,context_,&QObject::deleteLater);thread_.start();}
    ~MathRuntime(){QMetaObject::invokeMethod(context_,[this]{stop();},Qt::BlockingQueuedConnection);thread_.quit();thread_.wait();}
    QString convert(const QString& directory,const QString& node,const QJsonArray& formulas,QJsonArray& output){
        const auto input=QJsonDocument(formulas).toJson(QJsonDocument::Compact);
        if(directory_!=directory||node_!=node){stop();cache_.clear();directory_=directory;node_=node;}
        if(const auto cached=cache_.object(input)){output=*cached;return {};}
        if(!process_||process_->state()==QProcess::NotRunning){
            stop();process_=std::make_unique<QProcess>();process_->setWorkingDirectory(directory);
            process_->start(node,{directory+"/tex-svg.cjs","--worker"});
            if(!process_->waitForStarted(3000)){stop();return "Não foi possível iniciar o runtime local de matemática.";}
        }
        process_->write(input+'\n');
        QElapsedTimer timer;timer.start();
        while(!process_->canReadLine()){
            const auto remaining=15000-timer.elapsed();
            if(remaining<=0){stop();return "A fórmula excedeu o tempo de conversão.";}
            if(!process_->waitForReadyRead(int(remaining))&&!process_->canReadLine()){
                const auto detail=QString::fromUtf8(process_->readAllStandardError()).left(300);
                const bool timedOut=process_->state()!=QProcess::NotRunning;
                stop();return timedOut ? QStringLiteral("A fórmula excedeu o tempo de conversão.") : QStringLiteral("Falha no MathJax local: ")+detail;
            }
        }
        const auto document=QJsonDocument::fromJson(process_->readLine());
        if(!document.isObject()){stop();return "Resposta inválida do MathJax local.";}
        const auto response=document.object();
        if(response.contains("error"))return "LaTeX inválido: "+response["error"].toString().left(300);
        if(!response["result"].isArray()){stop();return "Resposta inválida do MathJax local.";}
        output=response["result"].toArray();
        if(output.size()!=formulas.size())return "Resposta inválida do MathJax local.";
        for(const auto& value:output){const auto o=value.toObject();const auto w=o["widthEm"].toDouble(),h=o["heightEm"].toDouble();
            if(!validMathSvg(o["svg"].toString().toStdString())||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||w>2000||h>2000)return "SVG matemático inválido.";
        }
        cache_.insert(input,new QJsonArray(output),int(input.size()+QJsonDocument(output).toJson(QJsonDocument::Compact).size()));
        return {};
    }
    QString render(const QString& directory,const QString& node,const QJsonArray& formulas,QJsonArray& output){
        QString error;QMetaObject::invokeMethod(context_,[&]{error=convert(directory,node,formulas,output);},Qt::BlockingQueuedConnection);return error;
    }
    void warm(const QString& directory,const QString& node){QMetaObject::invokeMethod(context_,[this,directory,node]{QJsonArray output;convert(directory,node,{},output);},Qt::QueuedConnection);}
};
MathRuntime& mathRuntime(){static MathRuntime runtime;return runtime;}
QString mathNode(const QString& directory){
#ifdef Q_OS_WIN
    const QString executable="node.exe";
#else
    const QString executable="node";
#endif
    const auto local=directory+"/"+executable;return QFile::exists(local)?local:QStandardPaths::findExecutable(executable);
}
}
std::vector<MathFragment> mathFragments(const std::string& source){
    std::vector<MathFragment> result;
    for(std::size_t i=0;i<source.size();++i){
        if(source[i]=='\\'){++i;continue;}
        if(source[i]!='$')continue;
        const std::size_t n=i+1<source.size()&&source[i+1]=='$'?2:1;
        auto end=i+n;
        for(;end<source.size();++end){if(source[end]=='\\'){++end;continue;}if(source[end]=='$'&&(n==1||(end+1<source.size()&&source[end+1]=='$')))break;}
        if(end>=source.size())continue;
        if(end>i+n)result.push_back({source.substr(i+n,end-i-n),{},n==2,i,end+n-i,0,0});
        i=end+n-1;
    }return result;
}
QString mathRuntimeDirectory(){
    const auto env=qEnvironmentVariable("SCALAR_MATH_RUNTIME");
    for(const auto& path:QStringList{env,QCoreApplication::applicationDirPath()+"/math",QCoreApplication::applicationDirPath()+"/../share/scalar/math",QStringLiteral(SCALAR_MATH_SOURCE_DIR)})
        if(!path.isEmpty()&&QFile::exists(path+"/tex-svg.cjs"))return QDir(path).absolutePath();
    return {};
}
QString renderMath(std::vector<MathFragment>& fragments){
    if(fragments.empty())return {};
    const auto dir=mathRuntimeDirectory();
    const auto node=mathNode(dir);
    if(dir.isEmpty()||node.isEmpty()||!QFile::exists(dir+"/node_modules/mathjax-full/js/mathjax.js"))return "O pacote local de matemática está ausente. Reinstale o pacote completo do Scalar.";
    QJsonArray formulas;for(const auto& f:fragments)formulas.append(QJsonObject{{"latex",QString::fromStdString(f.latex)},{"display",f.display}});
    QJsonArray output;const auto error=mathRuntime().render(dir,node,formulas,output);if(!error.isEmpty())return error;
    for(qsizetype i=0;i<output.size();++i){auto& f=fragments[std::size_t(i)];const auto o=output[i].toObject();
        f.svg=o["svg"].toString().toStdString();f.widthEm=o["widthEm"].toDouble();f.heightEm=o["heightEm"].toDouble();
        if(!validMathSvg(f.svg)||!std::isfinite(f.widthEm)||!std::isfinite(f.heightEm)||f.widthEm<=0||f.heightEm<=0||f.widthEm>2000||f.heightEm>2000)return "SVG matemático inválido.";
    }return {};
}
void warmMathRuntime(){
    const auto directory=mathRuntimeDirectory(),node=mathNode(directory);
    if(!directory.isEmpty()&&!node.isEmpty()&&QFile::exists(directory+"/node_modules/mathjax-full/js/mathjax.js"))mathRuntime().warm(directory,node);
}
}
