#include "mainwnd.h"
#include "ui_mainwnd.h"

//for error alerts
template<typename en>
QString enum_name(en enum_value){
    return QString::fromStdString(std::string(magic_enum::enum_name<en>(enum_value)));
}

void MainWnd::fastAlert(const QString& str) {
    msgBox->setText(str);
    msgBox->show();
}

void MainWnd::safeSliderValueSet(int value){
    ui->time_slider->blockSignals(true);
    ui->time_slider->setValue(value);
    ui->time_slider->blockSignals(false);
}

//all-in-one
void MainWnd::initEverything(){
    if(!deviceWrapper.device)
        return;
    openni::Status lastStatus = openni::Status::STATUS_OK;
    try{
        lastStatus = deviceWrapper.depthStream->create(*deviceWrapper.device, openni::SENSOR_DEPTH);
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("depthStream was not created: " + enum_name<decltype(lastStatus)>(lastStatus));
            return;
        }
        lastStatus = deviceWrapper.depthStream->start();
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("depthStream didn't start: " + enum_name<decltype(lastStatus)>(lastStatus));
            return;
        }
    }
    catch(...){
        fastAlert("depthStream failed starting: " + enum_name<decltype(lastStatus)>(lastStatus));
    }
    try {
        lastStatus = deviceWrapper.device->setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("colorStream setImageRegistrationMode: " + enum_name<decltype(lastStatus)>(lastStatus));
            return;
        }
        lastStatus = deviceWrapper.colorStream->create(*deviceWrapper.device, openni::SENSOR_COLOR);
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("colorStream was not created: " + enum_name<decltype(lastStatus)>(lastStatus));
            return;
        }
        lastStatus = deviceWrapper.colorStream->start();
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("colorStream didn't start: " + enum_name<decltype(lastStatus)>(lastStatus));
            return;
        }
    }
    catch (...) {
        fastAlert("colorStream failed starting: " + enum_name<decltype(lastStatus)>(lastStatus));
    }

    // here i could seek through videostream, caching frames on a fly etc. Yet, it was not really possible
    std::thread th([this](){
        auto ans = deviceWrapper.prepareQPixmaps();
        if(ans != deviceVStreamInfo::RefillingStatus::OK){
            deviceWrapper.lastReadyFrame = -1;
        }
    });
    th.detach();

    //checking if first frame is ready
    deviceWrapper.firstFrameReady.lock();
    deviceWrapper.firstFrameReady.unlock();

    ui->time_slider->setMinimum(0);
    safeSliderValueSet(0);

    ui->left_label->setText("Loading...");

    if(!repeater){
        repeater = new Repeater([this](){
            if(deviceWrapper.lastReadyFrame<0)
                return;//if nothing to render -> exit

            if(deviceWrapper.readyForUsage){
                if(!ui->butt_frame->isEnabled()){
                    ui->left_label->setText("ONI Loaded...");

                    safeSliderValueSet(0);
                    ui->time_slider->setMaximum(deviceWrapper.lastReadyFrame);
                    restartPlaybackFromPos(0);

                    setEnabledUi(true);
                }
            }
            else{
                ui->left_label->setText("Loading... "+QString::number(float_t(deviceWrapper.lastReadyFrame*100)/deviceWrapper.depthPixmaps.size())+"%");
                return;
            }

            auto now = std::chrono::steady_clock::now();
            auto timeSincePlaybackStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - playbackStartTime->first).count();
            int64_t frameNo = 0;

            if(playbackEnabled)
                nextFrame = (frameNo = (timeSincePlaybackStart*deviceWrapper.FPS)/1000 + playbackStartTime->second);
            else
                frameNo = nextFrame;

            if(!playbackEnabled && (frameNo == currentFrame || frameNo < 0 || frameNo > deviceWrapper.lastReadyFrame)){
                nextFrame = currentFrame;
                return;
            }

            if(playbackEnabled && frameNo > deviceWrapper.lastReadyFrame){
                frameNo = deviceWrapper.lastReadyFrame;
                playbackEnabled = false;
            }

            currentFrame = frameNo;
            ui->right_label->setText(buildTimeString(currentFrame/deviceWrapper.FPS)+QString(" (F%1)").arg(currentFrame));

            float_t framePos = float_t(frameNo)/deviceWrapper.lastReadyFrame;
            safeSliderValueSet(framePos*ui->time_slider->maximum());

            setFrameByPosition(framePos);
        },33);
    }
}

QString MainWnd::buildTimeString(int64_t seconds){
    QString empty = "", zero = "0";
    auto sec = seconds%60;
    auto min = (seconds%3600)/60;
    auto hr = seconds/3600;
    return (((hr<=9?zero:empty))+QString::number(hr)+":"+
            ((min<=9?zero:empty))+QString::number(min)+":"+
            ((sec<=9?zero:empty))+QString::number(sec));
}

void MainWnd::restartPlaybackFromPos(float_t pos){
    playbackStartTime->second = deviceWrapper.lastReadyFrame*pos;
    playbackStartTime->first = std::chrono::steady_clock::now();
    playbackEnabled = true;
}

void MainWnd::restartPlaybackFromFrame(int64_t frame){
    restartPlaybackFromPos(float_t(frame)/deviceWrapper.lastReadyFrame);
}

void MainWnd::setFrameByPosition(float_t pos){
    auto destFrame = size_t(deviceWrapper.lastReadyFrame*pos);

    leftScene->removeItem(previousLeftPixmap);
    rightScene->removeItem(previousRightPixmap);

    leftScene->addItem(previousLeftPixmap = deviceWrapper.colorPixmaps[destFrame]);
    rightScene->addItem(previousRightPixmap = deviceWrapper.depthPixmaps[destFrame]);

    ui->left_gview->fitInView(previousLeftPixmap,Qt::KeepAspectRatio);
    ui->right_gview->fitInView(previousRightPixmap,Qt::KeepAspectRatio);

    if(firstRun){
        firstRun = false;
        ui->left_gview->setScene(leftScene);
        ui->right_gview->setScene(rightScene);
    }
}

void MainWnd::Play(){
    float_t sliderPos = float_t(ui->time_slider->value())/ui->time_slider->maximum();
    if(playbackEnabled && sliderPos == 1.)
        sliderPos = 0;
    restartPlaybackFromPos(sliderPos);
}
void MainWnd::Pause(){
    playbackEnabled = false;
    nextFrame = currentFrame;
}
void MainWnd::OneFrameForward(){
    playbackEnabled = false;
    nextFrame = currentFrame + 1;
}
void MainWnd::OneFrameBackward(){
    playbackEnabled = false;
    nextFrame = currentFrame - 1;
}
void MainWnd::FirstFrame(){
    if(playbackEnabled)
        restartPlaybackFromPos(0);
    else
        nextFrame = 0;
}
void MainWnd::LastFrame(){
    if(playbackEnabled)
        restartPlaybackFromPos(1);
    else
        nextFrame = deviceWrapper.lastReadyFrame;
}

void MainWnd::SliderMove(int value){
    if(playbackEnabled)
        restartPlaybackFromPos(float_t(value)/ui->time_slider->maximum());
    nextFrame = (value*deviceWrapper.lastReadyFrame)/ui->time_slider->maximum();
}

void MainWnd::reinititialiseComponents(){
    setEnabledUi(false);
    *playbackStartTime = {std::chrono::steady_clock::now(),0};

    auto leftSceneItems = leftScene->items();
    auto rightSceneItems = rightScene->items();

    for(auto& i: leftSceneItems)
        leftScene->removeItem(i);
    for(auto& i: rightSceneItems)
        rightScene->removeItem(i);

    previousLeftPixmap = nullptr;
    previousRightPixmap = nullptr;
}

void MainWnd::openFile() {
    bool lockObtained = deviceWrapper.fileProcessing.try_lock();
    if(!lockObtained){
        fastAlert("File is in the process of loading...");
        return;
    }
    else
        deviceWrapper.fileProcessing.unlock();
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(tr("ONI Files (*.oni)"));
    if (dialog.exec()){
        playbackEnabled = false;
        auto fileNames = dialog.selectedFiles();
        auto firstFile = fileNames.front();
        auto filename = (firstFile).toUtf8();
        auto devicePtr = new openni::Device();
        auto openStatus = devicePtr->open(filename.constData());
        if(openStatus!=openni::STATUS_OK){
            fastAlert("Device failed to open: " + enum_name<openni::Status>(openStatus));
            delete devicePtr;
            return;
        }
        reinititialiseComponents();
        deviceWrapper.clearAll();
        deviceWrapper.device = devicePtr;
        deviceWrapper.playbackControl = deviceWrapper.device->getPlaybackControl();
        initEverything();
    }
    else{
        //something
    }
}

void MainWnd::setEnabledUi(bool enable){
    ui->center->setEnabled(enable);
    ui->butt_frame->setEnabled(enable);
    ui->time_slider->setEnabled(enable);
    ui->pause_button->setEnabled(enable);
    ui->play_button->setEnabled(enable);
    ui->next_frame->setEnabled(enable);
    ui->prev_frame->setEnabled(enable);
    ui->first_frame->setEnabled(enable);
    ui->last_frame->setEnabled(enable);
    ui->right_label->setEnabled(enable);
    ui->left_label->setEnabled(enable);
}

MainWnd::MainWnd(QWidget *parent) :
    QMainWindow(parent),
    repeater(nullptr),
    leftScene(new QGraphicsScene(this)),
    rightScene(new QGraphicsScene(this)),
    ui(new Ui::MainWnd),
    msgBox(new QMessageBox(this)),
    previousLeftPixmap(nullptr),
    previousRightPixmap(nullptr),
    playbackStartTime(new time_frame_pair({std::chrono::steady_clock::now(),0})),
    currentFrame(0), nextFrame(0),
    playbackEnabled(false), firstRun(true) {

    ui->setupUi(this);
    msgBox->setIcon(QMessageBox::Warning);
    setEnabledUi(false);

    auto openFileButtStatus = connect(ui->actionOpen,SIGNAL(triggered()), this,SLOT(openFile()));
    auto playButtStatus = connect(ui->play_button,SIGNAL(clicked()),this, SLOT(Play()));
    auto pauseButtStatus = connect(ui->pause_button,SIGNAL(clicked()),this, SLOT(Pause()));
    auto prevFrameButtStatus = connect(ui->next_frame,SIGNAL(clicked()),this, SLOT(OneFrameForward()));
    auto nextFrameButtStatus = connect(ui->prev_frame,SIGNAL(clicked()),this, SLOT(OneFrameBackward()));
    auto firstFrameButtStatus = connect(ui->first_frame,SIGNAL(clicked()),this, SLOT(FirstFrame()));
    auto lastFrameButtStatus = connect(ui->last_frame,SIGNAL(clicked()),this, SLOT(LastFrame()));
    auto sliderMoveStatus = connect(ui->time_slider,SIGNAL(valueChanged(int)),this,SLOT(SliderMove(int)));

    try {
        openni::OpenNI::initialize();
    }
    catch(...) {
        fastAlert(openni::OpenNI::getExtendedError());
    }
}

MainWnd::~MainWnd() {
    openni::OpenNI::shutdown();
}

