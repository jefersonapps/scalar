#pragma once
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>

namespace scalar {
// Opt-in diagnostics shared by the GUI and scene-graph threads. No file or timer
// is opened in normal operation. Only input/tool state is recorded, never page content.
class InputDiagnostics {
public:
    static bool enabled(){static const bool value=!qEnvironmentVariableIsEmpty("SCALAR_TABLET_TRACE");return value;}
    static void write(QJsonObject row){
        if(!enabled())return;
        struct Sink {
            QMutex mutex;
            QFile file{qEnvironmentVariable("SCALAR_TABLET_TRACE")};
            QElapsedTimer clock;
            Sink(){clock.start();file.open(QIODevice::WriteOnly|QIODevice::Truncate);}
        };
        static Sink sink;
        QMutexLocker lock(&sink.mutex);
        if(!sink.file.isOpen()||sink.file.pos()>16*1024*1024)return;
        row["trace_us"]=double(sink.clock.nsecsElapsed()/1000);
        sink.file.write(QJsonDocument(row).toJson(QJsonDocument::Compact));sink.file.write("\n");
        sink.file.flush();
    }
};
class InputDiagnosticSpan {
public:
    explicit InputDiagnosticSpan(const char* phase,qint64 thresholdUs=0):phase_(phase),thresholdUs_(thresholdUs){if(InputDiagnostics::enabled())timer_.start();}
    ~InputDiagnosticSpan(){
        if(!timer_.isValid())return;
        const auto elapsed=timer_.nsecsElapsed()/1000;
        if(elapsed>=thresholdUs_)InputDiagnostics::write({{"phase",phase_},{"duration_us",double(elapsed)}});
    }
private:
    const char* phase_;
    qint64 thresholdUs_;
    QElapsedTimer timer_;
};
}
