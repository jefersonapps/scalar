#pragma once
#include "documents/Document.h"
#include <QObject>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QTimer>
#include <QRectF>
#include <QUrl>
#include <deque>
namespace scalar {
class ConstructionRecorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(bool exporting READ exporting NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int position READ position WRITE seek NOTIFY changed)
    Q_PROPERTY(QUrl frame READ frame NOTIFY changed)
    Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool encoderAvailable READ encoderAvailable CONSTANT)
public:
    explicit ConstructionRecorder(QObject* parent=nullptr);
    ~ConstructionRecorder() override;
    bool recording() const{return recording_;}
    bool pending() const{return captureBusy_||!queue_.empty();}
    bool playing() const{return timer_.isActive();}
    bool exporting() const{return exportWorker_.isRunning();}
    int count() const{return frames_.size();}
    int position() const{return position_;}
    QUrl frame() const{return position_>=0&&position_<count()?QUrl::fromLocalFile(frames_[position_]):QUrl{};}
    QString exportStatus() const{return exportStatus_;}
    QString error() const{return error_;}
    bool encoderAvailable() const;
    Page* replayPage(){return replay_ ? &*replay_ : nullptr;}
    const Page* replayPage() const{return replay_ ? &*replay_ : nullptr;}
    void start(const Page& page,QRectF region);
    void capture(const Page& page);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(int position);
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void closePreview();
    Q_INVOKABLE void exportAnimation(const QUrl& destination,const QString& format);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void deleteStep(int index);
signals:
    void changed();
private:
    void process();
    QTemporaryDir directory_;
    std::deque<Page> queue_;
    QStringList frames_;
    QFutureWatcher<QString> worker_,exportWorker_;
    QTimer timer_;
    QRectF region_;
    std::string pageId_;
    QString error_,exportStatus_;
    int position_=-1,generation_=0;
    std::optional<Page> replay_;
    bool recording_=false,captureBusy_=false;
};
}
