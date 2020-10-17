#ifndef MAINWND_H
#define MAINWND_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QGraphicsView>
#include <QPixmap>
#include <QGraphicsPixmapItem>

#include "Include/OpenNI.h"

#include "device_vstream_info.h"
#include "repeater.h"

#include <chrono>

#include "magic_enum.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWnd;
}
QT_END_NAMESPACE

class MainWnd : public QMainWindow {
    Q_OBJECT

    using time_frame_pair = std::pair<std::chrono::time_point<std::chrono::steady_clock>, int64_t>;
public:
    MainWnd(QWidget *parent = nullptr);
    ~MainWnd();
    void fastAlert(const QString& str);
    void setEnabledUi(bool enable);
    void setFrameByPosition(float_t pos);
    void restartPlaybackFromPos(float_t pos);
    void restartPlaybackFromFrame(int64_t pos);
    QString buildTimeString(int64_t seconds);
    void reinititialiseComponents();
    void safeSliderValueSet(int value);
private slots:
    void openFile();
    void initEverything();
    void Play();
    void Pause();
    void OneFrameForward();
    void OneFrameBackward();
    void FirstFrame();
    void LastFrame();
    void SliderMove(int value);
private:
    std::unique_ptr<Repeater> repeater;
    std::unique_ptr<QGraphicsScene> leftScene;
    std::unique_ptr<QGraphicsScene> rightScene;
    std::unique_ptr<Ui::MainWnd> ui;
    std::unique_ptr<QMessageBox> msgBox;
    deviceVStreamInfo deviceWrapper;
    std::unique_ptr<QGraphicsPixmapItem> previousLeftPixmap;
    std::unique_ptr<QGraphicsPixmapItem> previousRightPixmap;

    std::unique_ptr<time_frame_pair> playbackStartTime;
    int64_t currentFrame;
    int64_t nextFrame;
    bool playbackEnabled, firstRun;

    QMutex mutex;
};
#endif // MAINWND_H
