#include "mainwnd.h"
#include "ui_mainwnd.h"

template<typename en>
std::string enum_name(en enum_value){
    return std::string(magic_enum::enum_name<en>(enum_value));
}

template<typename en>
QString q_enum_name(en enum_value){
    return QString::fromStdString(enum_name(enum_value));
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

void MainWnd::initEverything(){
    if(!deviceWrapper.device.get())
        return;
    setEnabledUi(false);
    openni::Status lastStatus = openni::Status::STATUS_OK;
    try{
        lastStatus = deviceWrapper.depthStream->create(*deviceWrapper.device, openni::SENSOR_DEPTH);
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("depthStream was not created: " + q_enum_name(lastStatus));
            return;
        }
        lastStatus = deviceWrapper.depthStream->start();
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("depthStream didn't start: " + q_enum_name(lastStatus));
            return;
        }
    }
    catch(...){
        fastAlert("depthStream failed starting: " + q_enum_name(lastStatus));
    }
    try {
        lastStatus = deviceWrapper.device->setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("colorStream setImageRegistrationMode: " + q_enum_name(lastStatus));
            return;
        }
        lastStatus = deviceWrapper.colorStream->create(*deviceWrapper.device, openni::SENSOR_COLOR);
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("colorStream was not created: " + q_enum_name(lastStatus));
            return;
        }
        lastStatus = deviceWrapper.colorStream->start();
        if(lastStatus != openni::Status::STATUS_OK){
            fastAlert("colorStream didn't start: " + q_enum_name(lastStatus));
            return;
        }
    }
    catch (...) {
        fastAlert("colorStream failed starting: " + q_enum_name(lastStatus));
    }

    deviceWrapper.updateInfo();

    std::cout << deviceWrapper.lastFrameInStreams << std::endl;

    ui->time_slider->setMinimum(0);
    safeSliderValueSet(0);

    ui->left_label->setText("Wait a moment...");

    if(!repeater.get()){
        repeater.reset(new EffectiveRepeater([this](){
            if(deviceWrapper.lastFrameInStreams<0)
                return;//if nothing to render -> exit

            if(deviceWrapper.readyForUsage){
                if(!ui->butt_frame->isEnabled()){
                    ui->left_label->setText("ONI Loaded...");

                    safeSliderValueSet(0);
                    ui->time_slider->setMaximum(deviceWrapper.lastFrameInStreams);
                    restartPlaybackFromPos(0);

                    setEnabledUi(true);
                }
            }
            else{
                ui->left_label->setText("Wait a moment... ");
                return;
            }

            auto now = std::chrono::steady_clock::now();
            auto timeSincePlaybackStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - playbackStartTime->first).count();
            int64_t frameNo = 0;

            if(playbackEnabled)
                nextFrame = (frameNo = (timeSincePlaybackStart*deviceWrapper.FPS)/1000 + playbackStartTime->second);
            else
                frameNo = nextFrame;

            if(!playbackEnabled && (frameNo == currentFrame || frameNo < 0 || frameNo > deviceWrapper.lastFrameInStreams)){
                nextFrame = currentFrame;
                return;
            }

            if(playbackEnabled && frameNo > deviceWrapper.lastFrameInStreams){
                frameNo = deviceWrapper.lastFrameInStreams;
                playbackEnabled = false;
            }

            currentFrame = frameNo;
            ui->right_label->setText(buildTimeString(currentFrame/deviceWrapper.FPS)+QString(" (F%1)").arg(currentFrame));

            float_t framePos = float_t(frameNo)/deviceWrapper.lastFrameInStreams;
            safeSliderValueSet(framePos*ui->time_slider->maximum());

            setFrameByPosition(framePos);
        },33));
    }
}

QString MainWnd::buildTimeString(int64_t seconds){
    QString empty = "", zero = "0";
    auto sec = seconds%60;
    auto min = (seconds%3600)/60;
    auto hr = seconds/3600;
    return (((hr<10?zero:empty))+QString::number(hr)+":"+
            ((min<10?zero:empty))+QString::number(min)+":"+
            ((sec<10?zero:empty))+QString::number(sec));
}

void MainWnd::restartPlaybackFromPos(float_t pos){
    playbackStartTime->second = deviceWrapper.lastFrameInStreams*pos;
    playbackStartTime->first = std::chrono::steady_clock::now();
    playbackEnabled = true;
}

void MainWnd::restartPlaybackFromFrame(int64_t frame){
    restartPlaybackFromPos(float_t(frame)/deviceWrapper.lastFrameInStreams);
}

//now bufferless and therefore slower (much slower)

//why second recording isn't working
void MainWnd::setFrameByPosition(float_t pos){
    size_t destFrame = size_t(deviceWrapper.lastFrameInStreams*pos);

    if(pos>1.f || pos<0)
        return;

    leftScene->removeItem(previousLeftPixmap.get());
    rightScene->removeItem(previousRightPixmap.get());

    auto seekColorFrameRes = deviceWrapper.getPlaybackControl()->seek(*deviceWrapper.colorStream,destFrame);
    auto seekDepthFrameRes = deviceWrapper.getPlaybackControl()->seek(*deviceWrapper.depthStream,destFrame);

    if(seekDepthFrameRes!=seekColorFrameRes && seekColorFrameRes != openni::STATUS_OK){
        fastAlert("Seek failure.");
        return;
    }

    auto depthReadStatus = deviceWrapper.depthStream->readFrame(&deviceWrapper.depthFrame);
    auto colorReadStatus = deviceWrapper.colorStream->readFrame(&deviceWrapper.colorFrame);

    if(depthReadStatus != colorReadStatus && colorReadStatus != openni::STATUS_OK){
        fastAlert("Frame reading failure.");
        return;
    }

    previousLeftPixmap.reset(deviceWrapper.createColorPixMapFromFrame(deviceWrapper.colorFrame));
    previousRightPixmap.reset(deviceWrapper.createDepthPixMapFromFrame(deviceWrapper.depthFrame));

    leftScene->addItem(previousLeftPixmap.get());
    rightScene->addItem(previousRightPixmap.get());

    ui->left_gview->fitInView(previousLeftPixmap.get(),Qt::KeepAspectRatio);
    ui->right_gview->fitInView(previousRightPixmap.get(),Qt::KeepAspectRatio);

    if(firstRun){
        firstRun = false;
        ui->left_gview->setScene(leftScene.get());
        ui->right_gview->setScene(rightScene.get());
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
        nextFrame = deviceWrapper.lastFrameInStreams;
}

void MainWnd::SliderMove(int value){
    if(playbackEnabled)
        restartPlaybackFromPos(float_t(value)/ui->time_slider->maximum());
    nextFrame = (value*deviceWrapper.lastFrameInStreams)/ui->time_slider->maximum();
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

    previousLeftPixmap.reset(new QGraphicsPixmapItem);
    previousRightPixmap.reset(new QGraphicsPixmapItem);
}

void MainWnd::openFile() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(tr("ONI Files (*.oni)"));
    if (dialog.exec()){
        playbackEnabled = false;
        auto fileNames = dialog.selectedFiles();
        auto firstFile = fileNames.front();
        auto filename = (firstFile).toUtf8();
        auto devicePtr = std::unique_ptr<openni::Device>(new openni::Device());
        auto openStatus = devicePtr->open(filename.constData());
        if(openStatus!=openni::STATUS_OK){
            fastAlert("Device failed to open: " + q_enum_name<openni::Status>(openStatus));
            devicePtr.reset();
            return;
        }
        reinititialiseComponents();
        deviceWrapper.clearAll();
        deviceWrapper.device.swap(devicePtr);
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
    leftScene(new QGraphicsScene(this)),
    rightScene(new QGraphicsScene(this)),
    ui(new Ui::MainWnd),
    msgBox(new QMessageBox(this)),
    previousLeftPixmap(new QGraphicsPixmapItem),
    previousRightPixmap(new QGraphicsPixmapItem),
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

