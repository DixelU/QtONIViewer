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

#include <QWidget>
#include <QImage>
#include <QPainter>

struct deviceVStreamInfo{
    enum class RefillingStatus{
        OK,
        NULL_POINTERS,
        NO_VALID_STREAMS,
        FRAME_READING_FAILURE
    };
    std::unique_ptr<openni::Device> device;
    std::unique_ptr<openni::VideoStream> depthStream;
    std::unique_ptr<openni::VideoStream> colorStream;
    openni::VideoFrameRef depthFrame, colorFrame;
    int64_t FPS;
    int64_t lastFrameInStreams;
    bool readyForUsage;
    deviceVStreamInfo():
        depthStream(new openni::VideoStream),
        colorStream(new openni::VideoStream),
        FPS(0), lastFrameInStreams(-1), readyForUsage(false)
    {}
    ~deviceVStreamInfo(){
        clearAll(true);
    }
    RefillingStatus updateInfo(){
        if(!device.get())
            return RefillingStatus::NULL_POINTERS;
        if(!depthStream.get() || !colorStream.get())
            return RefillingStatus::NULL_POINTERS;
        if(!depthStream->isValid() || !colorStream->isValid())
            return RefillingStatus::NO_VALID_STREAMS;
        size_t depthFramesCount = getPlaybackControl()->getNumberOfFrames(*depthStream);
        size_t colorFramesCount = getPlaybackControl()->getNumberOfFrames(*colorStream);
        lastFrameInStreams =  min(depthFramesCount,colorFramesCount) - 1;
        FPS = colorStream->getVideoMode().getFps();
        readyForUsage = true;
        return RefillingStatus::OK;
    }
    void clearFrameBuffer(){
        lastFrameInStreams = -1;
        readyForUsage = false;
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
            device->close();
            device.reset();
        }
        colorStream.reset();
        depthStream.reset();
        if(!isDestruction){
            FPS = 0;
            lastFrameInStreams = -1;
            depthStream.reset(new openni::VideoStream);
            colorStream.reset(new openni::VideoStream);
            readyForUsage = false;
        }
    }
    inline openni::PlaybackControl* getPlaybackControl(){
        return device->getPlaybackControl();
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
