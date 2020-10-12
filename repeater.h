#ifndef REPEATER_H
#define REPEATER_H

#include <functional>
#include <QTimer>

class Repeater : public QObject {
private:
    Q_OBJECT
    std::function<void()> func;
    QTimer timer;
public slots:
    void repeatingFunc(){
        func();
    }
public:
    Repeater(const std::function<void()>& func, int64_t periodMS): func(func), timer(this) {
        connect(&timer, SIGNAL(timeout()), this, SLOT(repeatingFunc()));
        timer.start(periodMS);
    }
    ~Repeater(){
        timer.stop();
        disconnect(&timer, SIGNAL(timeout()), this, SLOT(repeatingFunc()));
    }
};

#endif // REPEATER_H
