#ifndef REPEATER_H
#define REPEATER_H

#include <functional>
#include <chrono>
#include <QTimer>

class Repeater : public QObject {
    Q_OBJECT
protected:
    float_t periodMS;
    std::function<void()> func;
public slots:
    virtual void repeatingFunc(){
        func();
        setDelayedStart();
    }
public:
    Repeater(const std::function<void()>& func, float_t periodMS): periodMS(periodMS),func(func) {
    }
    inline float_t getPeriod(){
        return periodMS;
    }
    virtual void setDelayedStart(){
        QTimer::singleShot(periodMS, this, SLOT(repeatingFunc()));
    }
    ~Repeater(){
    }
    void reassignPeriod(float_t newPeriodMS){
        periodMS = newPeriodMS;
    }
};

#define REASSIGN_BASED_ON_AVERAGE_EXEC_TIME
class EffectiveRepeater : public Repeater{
    Q_OBJECT
    int overheadTime;

#ifdef REASSIGN_BASED_ON_AVERAGE_EXEC_TIME
    constexpr static int chainLength = 5;
    void putNewEvalTime(float_t ms){
        periodMS = (periodMS*(chainLength-1) + ms)/chainLength;
    }
#endif

public:

    EffectiveRepeater(const std::function<void()>& func, int64_t initialPeriodMS, int overheadTime):
        Repeater(func,initialPeriodMS), overheadTime(overheadTime){}

#ifdef REASSIGN_BASED_ON_AVERAGE_EXEC_TIME
    inline float_t getAverageExecTime() const { //
        return periodMS;
    }
#endif

    void setDelayedStart() override {
        QTimer::singleShot(periodMS, this, SLOT(repeatingFunc()));
    }

public slots:
    void repeatingFunc() override {

#ifdef REASSIGN_BASED_ON_AVERAGE_EXEC_TIME
        auto before = std::chrono::steady_clock::now();
#endif
        func();
        int time = overheadTime;

#ifdef REASSIGN_BASED_ON_AVERAGE_EXEC_TIME
        auto after =  std::chrono::steady_clock::now();
        putNewEvalTime(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count());
        time += getAverageExecTime();
#endif

        periodMS = time;
        setDelayedStart();
    }
};

#endif // REPEATER_H
