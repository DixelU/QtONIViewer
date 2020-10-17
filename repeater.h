#ifndef REPEATER_H
#define REPEATER_H

#include <functional>
#include <chrono>
#include <QTimer>

class Repeater : public QObject {
    Q_OBJECT
    QTimer timer;
protected:
    std::function<void()> func;
public slots:
    virtual void repeatingFunc(){
        func();
    }
public:
    Repeater(const std::function<void()>& func, int64_t periodMS): timer(this),func(func) {
        connect(&timer, SIGNAL(timeout()), this, SLOT(repeatingFunc()));
        timer.start(periodMS);
    }
    ~Repeater(){
        timer.stop();
        disconnect(&timer, SIGNAL(timeout()), this, SLOT(repeatingFunc()));
    }
    void reassignPeriod(int64_t periodMS){
        timer.stop();
        timer.start(periodMS);
    }
};

class EffectiveRepeater : public Repeater{
    Q_OBJECT
    constexpr static int chainLength = 4;
    constexpr static int additionalTime = 1;
    int64_t evalTime[chainLength];
    void putNewEvalTime(int64_t ms){
        for(int i=1;i<chainLength;i++)
            evalTime[i] = evalTime[i-1];
        evalTime[0] = ms;
    }
public:
    float_t avarageEvalTime() const {
        float_t avg = 0;
        for(int i=0;i<chainLength;i++)
            avg += evalTime[i];
        return avg/chainLength;
    }
    EffectiveRepeater(const std::function<void()>& func, int64_t initialPeriodMS):Repeater(func,initialPeriodMS){
        for(int i=0;i<chainLength;i++)
            evalTime[i] = initialPeriodMS;
    }
public slots:
    void repeatingFunc() override {
        auto before = std::chrono::steady_clock::now();
        func();
        auto after =  std::chrono::steady_clock::now();
        putNewEvalTime(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count());
        reassignPeriod(avarageEvalTime() + additionalTime);
    }
};

#endif // REPEATER_H
