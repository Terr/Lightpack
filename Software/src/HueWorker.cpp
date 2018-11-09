#include "hueworker.hpp"

#include <QDebug>
#include "PrismatikMath.hpp"
#include <QtNetwork>
#include "Settings.hpp"

using namespace SettingsScope;


HueWorker::HueWorker(const QList<StructRgb> *colors)
    :
      m_colorPtr(colors),
      m_isBusy(false),
      m_HueURL(Settings::getHueLightsURL()),
      m_repeatedWaitForResponseTimeMs(Settings::getRepeatedWaitForResponseTimeMs()),
      m_initialWaitForResponseTimeMs(Settings::getInitialWaitForResponeTimeMs())
{
    qDebug() << "HueWorker: started";
}

/*
 * Scale the brightness up so that 30% brightness of colors means 100% brightness on the Hue lights.
 * This negates the effect that the Hue will appear dark most of the time but will dimm the lights for very dark (black) scenes
 */
static double brightnessHeuristic(double Y)
{
    if(Y >= 30)
    {
        return 255;
    }
    return round((Y / 30) * 255);
}

void HueWorker::setHueColorsInner()
{
    qDebug() << "HueWorker: calculate average color and send to Hue Lights";
    StructXyz xyzColors = PrismatikMath::toXyz(PrismatikMath::avgColor(*m_colorPtr));
    double x = xyzColors.x / (xyzColors.x + xyzColors.y + xyzColors.z);
    double y = xyzColors.y / (xyzColors.x + xyzColors.y + xyzColors.z);
    qDebug() << "X: "  << xyzColors.x << "\nY:" << xyzColors.y << "\nZ: " << xyzColors.z;

    QNetworkRequest request((QUrl(m_HueURL)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QJsonObject json;
    QJsonArray xy;
    xy << x << y;
    json.insert("xy", xy);
    json.insert("bri", brightnessHeuristic(xyzColors.y));
    QNetworkAccessManager nam;

    qDebug() << "HueWorker: sending request";
    QNetworkReply *reply = nam.put(request, QJsonDocument(json).toJson());
    qDebug() << "HueWorker: waiting for reply of Hue Bridge";

    // sleep for (Default 75) ms since a reply is not expected much earlier
    QThread::msleep(m_initialWaitForResponseTimeMs);

    m_isBusy = true;
    while(!reply->isFinished())
    {
        QThread::msleep(m_repeatedWaitForResponseTimeMs);
        QCoreApplication::processEvents();
    }
    m_isBusy = false;
    qDebug() << "HueWorker: got reply of Hue Bridge";
    reply->deleteLater();


}
void HueWorker::setHueColors()
{
    qDebug() << "HueWorker: signaled to change color";
    if(m_isBusy)
    {
        qDebug() << "HueWorker: is busy, try again later";
    }
    else
    {
        setHueColorsInner();
    }
}
