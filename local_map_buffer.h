#ifndef THREAD_BUFFER_H
#define THREAD_BUFFER_H

#include <unordered_map>
#include <optional>
#include <thread>
#include <deque>

#include <QThread>
#include <QMutex>
#include <QObject>

#include "Include/OpenNI.h"

class ONILocalMapBuffer: QObject{
    Q_OBJECT
    std::shared_ptr<openni::VideoStream> videoStream;
    std::unordered_map<int,openni::VideoFrameRef> bufferedFrames;
    int reservedLocalCapacity;
    int firstNonInitialisedFrame;
    QMutex mutex;
public:
    ONILocalMapBuffer(std::shared_ptr<openni::VideoStream> videoStream, int reservedLocalCapacity) :
        videoStream(videoStream),
        reservedLocalCapacity(reservedLocalCapacity),
        firstNonInitialisedFrame(0) {

    }
    ~ONILocalMapBuffer(){ /*idk*/ }
public slots:
    std::optional<openni::VideoFrameRef> ExpandBufferAt(int frameNo){
        mutex.lock();
        if(!videoStream->isValid()){
            mutex.unlock();
            return {};
        }
        auto bufferedFrameAt = bufferedFrames.find(frameNo);
        if(bufferedFrameAt==bufferedFrames.end()){
            for(int frameIndex=frameNo;frameIndex<reservedLocalCapacity+frameNo;frameIndex++){
                if(bufferedFrames.find(frameNo)!=bufferedFrames.end())
                    continue;
                bufferedFrames[frameIndex] = openni::VideoFrameRef();
                auto readStatus = videoStream->readFrame(&bufferedFrames[frameIndex]);
                if(readStatus!=openni::STATUS_OK){
                    mutex.unlock();
                    if(frameIndex==frameNo)
                        return {};
                    return bufferedFrames[frameNo];
                }
                if(firstNonInitialisedFrame<=frameIndex)
                    firstNonInitialisedFrame = frameIndex;
            }
            bufferedFrameAt = bufferedFrames.find(frameNo);
        }
        mutex.unlock();
        return bufferedFrameAt->second;
    }
    void StreamClosed(std::shared_ptr<openni::VideoStream> videoStreamReplacement){
        mutex.lock();
        videoStream.swap(videoStreamReplacement);
        bufferedFrames.clear();
        firstNonInitialisedFrame = 0;
        mutex.unlock();
    }
    std::optional<openni::VideoFrameRef> BufferMove(int frameNo){
        mutex.lock();
        if(!videoStream->isValid()){
            mutex.unlock();
            return {};
        }
        if(frameNo<firstNonInitialisedFrame){
            auto bufferedFrameAt = bufferedFrames.find(frameNo);
            if(bufferedFrameAt==bufferedFrames.end())
                uncheckedExpandBufferAt(frameNo,reservedLocalCapacity);
        }
        auto frameRef = bufferedFrames[frameNo];
        mutex.unlock();
        return frameRef;
    }
private:
    void uncheckedExpandBufferAt(int frameNo,int expectedExpansionLength){
        for(int frameIndex=frameNo;frameIndex<expectedExpansionLength+frameNo;frameIndex++){
            if(bufferedFrames.find(frameNo)!=bufferedFrames.end())
                continue;
            if(firstNonInitialisedFrame<=frameIndex)
                firstNonInitialisedFrame = frameIndex;
            bufferedFrames[frameIndex] = openni::VideoFrameRef();
            auto readStatus = videoStream->readFrame(&bufferedFrames[frameIndex]);
            if(readStatus!=openni::STATUS_OK)
                return;
        }
        return;
    }
};

#endif // THREAD_BUFFER_H
