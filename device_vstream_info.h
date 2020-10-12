#ifndef DEVICE_VSTREAM_INFO_H
#define DEVICE_VSTREAM_INFO_H

#include <QMutex>
#include <QWidget>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>

#include <vector>
#include <iostream>
#include <string>
#include <thread>

#include "Include/OpenNI.h"

//if there could be more time, i'd prefer a different approach
struct deviceVStreamInfo{
    enum class RefillingStatus{
        OK,
        NULL_POINTERS,
        NO_VALID_STREAMS,
        FRAME_READING_FAILURE
    };
    openni::Device *device;
    openni::PlaybackControl *playbackControl;
    openni::VideoStream *depthStream;
    openni::VideoStream *colorStream;
    std::vector<QGraphicsPixmapItem*> depthPixmaps;
    std::vector<QGraphicsPixmapItem*> colorPixmaps;
    int64_t FPS;
    int64_t lastReadyFrame;
    bool readyForUsage;
    QMutex fileProcessing;
    QMutex firstFrameReady;
    deviceVStreamInfo():
        device(nullptr), playbackControl(nullptr),
        depthStream(new openni::VideoStream),
        colorStream(new openni::VideoStream),
        FPS(0), lastReadyFrame(-1), readyForUsage(false)
    {}
    ~deviceVStreamInfo(){
        clearAll(true);
    }
    RefillingStatus prepareQPixmaps(){
        if(!device || !playbackControl)
            return RefillingStatus::NULL_POINTERS;
        if(!depthStream || !colorStream)
            return RefillingStatus::NULL_POINTERS;
        if(!depthStream->isValid() || !colorStream->isValid())
            return RefillingStatus::NO_VALID_STREAMS;
        openni::VideoFrameRef depthFrame, colorFrame;
        firstFrameReady.lock();
        fileProcessing.lock();

        size_t depthFramesCount = playbackControl->getNumberOfFrames(*depthStream);
        size_t colorFramesCount = playbackControl->getNumberOfFrames(*colorStream);
        size_t frames_count = min(depthFramesCount,colorFramesCount);
        FPS = colorStream->getVideoMode().getFps();

        depthPixmaps.resize(frames_count,nullptr);
        colorPixmaps.resize(frames_count,nullptr);

        for(size_t curFrameIndex=0;curFrameIndex<frames_count;curFrameIndex++){
            auto depth_read_status = depthStream->readFrame(&depthFrame);
            auto color_read_status = colorStream->readFrame(&colorFrame);

            if(depth_read_status != color_read_status && color_read_status != openni::STATUS_OK){
                depthPixmaps.resize(curFrameIndex);
                colorPixmaps.resize(curFrameIndex);
                fileProcessing.unlock();
                return RefillingStatus::FRAME_READING_FAILURE;
            }
            if(!curFrameIndex)
                firstFrameReady.unlock();

            colorPixmaps[curFrameIndex] = (createColorPixMapFromFrame(colorFrame));
            depthPixmaps[curFrameIndex] = (createDepthPixMapFromFrame(depthFrame));

            lastReadyFrame = curFrameIndex;
        }
        readyForUsage = frames_count > 0;
        fileProcessing.unlock();
        return RefillingStatus::OK;
    }
    void clearFrameBuffer(){
        lastReadyFrame = -1;
        readyForUsage = false;
        for(auto& frame: depthPixmaps)
            delete frame;
        for(auto& frame: colorPixmaps)
            delete frame;
        colorPixmaps.clear();
        depthPixmaps.clear();
    }
    void clearAll(bool isDestruction=false){
        clearFrameBuffer();
        if(colorStream->isValid()){
            colorStream->stop();
            colorStream->destroy();
        }
        if(depthStream->isValid()){
            depthStream->stop();
            depthStream->destroy();
        }
        if(device){
            playbackControl = nullptr;
            device->close();
            delete device;
        }
        delete colorStream;
        delete depthStream;
        if(!isDestruction){
            FPS = 0;
            lastReadyFrame = -1;
            depthStream = new openni::VideoStream;
            colorStream = new openni::VideoStream;
            readyForUsage = false;
            device = nullptr;
            playbackControl = nullptr;
        }
    }
    inline QGraphicsPixmapItem* createColorPixMapFromFrame(openni::VideoFrameRef& frame){
        auto newPixmapItem = new QGraphicsPixmapItem();
        newPixmapItem->setPixmap(QPixmap::fromImage(QImage((uchar*)frame.getData(),frame.getWidth(),frame.getHeight(),QImage::Format_RGB888)));
        return newPixmapItem;
    }
    inline QGraphicsPixmapItem* createDepthPixMapFromFrame(openni::VideoFrameRef& frame){
        auto newPixmapItem = new QGraphicsPixmapItem();
        newPixmapItem->setPixmap(QPixmap::fromImage(QImage((uchar*)frame.getData(),frame.getWidth(),frame.getHeight(),QImage::Format_Grayscale16)));
        return newPixmapItem;
    }
};

#endif // DEVICE_VSTREAM_INFO_H
