#include "MathRenderer.h"
#include "SvgValidation.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
namespace scalar {
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
#ifdef Q_OS_WIN
    const QString executable="node.exe";
#else
    const QString executable="node";
#endif
    QString node=dir+"/"+executable;if(!QFile::exists(node))node=QStandardPaths::findExecutable(executable);
    if(dir.isEmpty()||node.isEmpty()||!QFile::exists(dir+"/node_modules/mathjax-full/js/mathjax.js"))return "O pacote local de matemática está ausente. Reinstale o pacote completo do Scalar.";
    QJsonArray formulas;for(const auto& f:fragments)formulas.append(QJsonObject{{"latex",QString::fromStdString(f.latex)},{"display",f.display}});
    QProcess process;process.setWorkingDirectory(dir);process.start(node,{dir+"/tex-svg.cjs"});
    if(!process.waitForStarted(3000))return "Não foi possível iniciar o runtime local de matemática.";
    process.write(QJsonDocument(formulas).toJson(QJsonDocument::Compact));process.closeWriteChannel();
    if(!process.waitForFinished(15000)){process.kill();process.waitForFinished();return "A fórmula excedeu o tempo de conversão.";}
    if(process.exitCode()!=0)return "LaTeX inválido: "+QString::fromUtf8(process.readAllStandardError()).left(300);
    const auto document=QJsonDocument::fromJson(process.readAllStandardOutput());const auto output=document.array();
    if(!document.isArray()||std::size_t(output.size())!=fragments.size())return "Resposta inválida do MathJax local.";
    for(qsizetype i=0;i<output.size();++i){auto& f=fragments[std::size_t(i)];const auto o=output[i].toObject();
        f.svg=o["svg"].toString().toStdString();f.widthEm=o["widthEm"].toDouble();f.heightEm=o["heightEm"].toDouble();
        if(!validMathSvg(f.svg)||!std::isfinite(f.widthEm)||!std::isfinite(f.heightEm)||f.widthEm<=0||f.heightEm<=0||f.widthEm>2000||f.heightEm>2000)return "SVG matemático inválido.";
    }return {};
}
}
