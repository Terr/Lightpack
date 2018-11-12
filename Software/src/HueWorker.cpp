#include "hueworker.hpp"

#include <QDebug>
#include "PrismatikMath.hpp"
#include "Settings.hpp"

using namespace SettingsScope;


HueWorker::HueWorker(const QList<StructRgb> *colors, QNetworkAccessManager* nam)
    :
      m_colorPtr(colors),
      m_nam(nam),
      m_isBusy(false),
      m_HueURL(Settings::getHueLightsURL()),
      m_maxBrightnessUntilThresholdValue(Settings::getMaxBrightnessUntilThresholdValue()),
      m_hueTransitionTime(Settings::getHueTransitionTime())
{
    qDebug() << "HueWorker: started";
}

/*
 * Scale the brightness up so that 30% brightness of colors means 100% brightness on the Hue lights.
 * This negates the effect that the Hue will appear dark most of the time but will dimm the lights for very dark (black) scenes
 */
static double brightnessHeuristic(double Y, unsigned int treshold)
{
    if(Y >= treshold)
    {
        return 255;
    }
    return round((Y / treshold) * 255);
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
    json.insert("transitiontime", (int)m_hueTransitionTime);    // temporary setting that will make the Philips Hue change colors faster
    json.insert("bri", brightnessHeuristic(xyzColors.y, m_maxBrightnessUntilThresholdValue));

    qDebug() << "HueWorker: sending request";
    m_isBusy = true;
    m_nam->put(request, QJsonDocument(json).toJson());
    qDebug() << "HueWorker: busy, waiting for reply of Hue Bridge";

    connect(m_nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
}

void HueWorker::replyFinished(QNetworkReply* reply)
{
    m_isBusy = false;
    reply->deleteLater();
    qDebug() << "HueWorker: finished request, not busy";
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
