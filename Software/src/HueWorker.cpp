#include "hueworker.hpp"

#include <QDebug>
#include "PrismatikMath.hpp"
#include <QtNetwork>


HueWorker::HueWorker(const QList<StructRgb> *colors) : m_colorPtr(colors), m_isBusy(false)
{
    qDebug() << "HueWorker: started";
}

void HueWorker::setHueColorsInner()
{
    qDebug() << "HueWorker: calculate average color";
    StructXyz xyzColors = PrismatikMath::toXyz(PrismatikMath::avgColor(*m_colorPtr));
    double x = xyzColors.x / (xyzColors.x + xyzColors.y + xyzColors.z);
    double y = xyzColors.y / (xyzColors.x + xyzColors.y + xyzColors.z);

    QNetworkRequest request(QUrl(""));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QJsonObject json;
    QJsonArray xy;
    xy << x << y;
    json.insert("xy", xy);

    QNetworkAccessManager nam;

    qDebug() << "HueWorker: sending request";
    QNetworkReply *reply = nam.put(request, QJsonDocument(json).toJson());
    qDebug() << "HueWorker: waiting for reply of Hue Bridge";

    QThread::msleep(75); // sleep for 75ms since a reply is not expected much earlier

    m_isBusy = true;
    while(!reply->isFinished())
    {
        QThread::msleep(25);
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
