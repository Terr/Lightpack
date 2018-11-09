#ifndef HUETHREAD_HPP
#define HUETHREAD_HPP

#include <QThread>
#include <QTimer>
#include "colorspace_types.h"

/*
 * This should be run in a new QThread - sends color updates to Hue lights.
 */
class HueWorker : public QObject
{
    Q_OBJECT
public:
    HueWorker() {}
    HueWorker(const QList<StructRgb> *colors);

public slots:
    void setHueColors();

protected:
    void setHueColorsInner();
    const QList<StructRgb> *m_colorPtr;
    bool m_isBusy;
};


#endif // HUETHREAD_HPP
