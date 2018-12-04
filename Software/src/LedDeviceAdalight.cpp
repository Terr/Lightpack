/*
 * LedDeviceAdalight.cpp
 *
 *	Created on: 06.09.2011
 *		Author: Mike Shatohin (brunql)
 *		Project: Lightpack
 *
 *	Lightpack is very simple implementation of the backlight for a laptop
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *	Lightpack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Lightpack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "LedDeviceAdalight.hpp"
#include "PrismatikMath.hpp"
#include "Settings.hpp"
#include "debug.h"
#include "stdio.h"
#include <QtSerialPort/QSerialPortInfo>

using namespace SettingsScope;

LedDeviceAdalight::LedDeviceAdalight(const QString &portName, const int baudRate, QObject *parent) : AbstractLedDevice(parent)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    m_portName = portName;
    m_baudRate = baudRate;

    //	m_gamma = Settings::getDeviceGamma();
    //	m_brightness = Settings::getDeviceBrightness();
    //	m_colorSequence =Settings::getColorSequence(SupportedDevices::DeviceTypeAdalight);
    m_AdalightDevice = NULL;

    // TODO: think about init m_savedColors in all ILedDevices
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << "initialized";
}

LedDeviceAdalight::~LedDeviceAdalight()
{
    close();
}

void LedDeviceAdalight::close()
{
    if (m_AdalightDevice != NULL) {
        m_AdalightDevice->close();

        delete m_AdalightDevice;
        m_AdalightDevice = NULL;
    }
}

static double getUpdatedValue(double from, double to, double step)
{
    if(from < to)
    {
        from += step;
        if(from > to)
        {
            return to;
        }
    }
    else
    {
        from -= step;
        if(from < to)
        {
            return to;
        }
    }
    return from;
}


void LedDeviceAdalight::updateSmoothColors()
{
    for(int i = 0; i < currColors.size(); i++)
    {
        // calculate updated intermediate color in Lab color space for a pleasent blending of colors
        labCur[i].l = getUpdatedValue(labCur[i].l, labTarget[i].l, step_sizes[i][0]);
        labCur[i].a = getUpdatedValue(labCur[i].a, labTarget[i].a, step_sizes[i][1]);
        labCur[i].b = getUpdatedValue(labCur[i].b, labTarget[i].b, step_sizes[i][2]);

        // update currColors with the resulting RGB values
        PrismatikMath::LabToXyz(labCur[i], xyzCur[i]);
        PrismatikMath::XyzToRgb(xyzCur[i], rgbCur[i]);
        currColors[i] = qRgb(rgbCur[i].r, rgbCur[i].g, rgbCur[i].b);
    }
}

void LedDeviceAdalight::updateSmoothColorsTick()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    // Save colors for showing changes of the brightness
    m_colorsSaved = currColors;

    resizeColorsBuffer(currColors.count());

    updateSmoothColors();

    applyColorModifications(currColors, m_colorsBuffer);

    m_writeBuffer.clear();
    m_writeBuffer.append(m_writeBufferHeader);

    for (int i = 0; i < m_colorsBuffer.count(); i++)
    {
        StructRgb color = m_colorsBuffer[i];

        color.r = color.r >> 4;
        color.g = color.g >> 4;
        color.b = color.b >> 4;

        if (m_colorSequence == "RBG")
        {
            m_writeBuffer.append(color.r);
            m_writeBuffer.append(color.b);
            m_writeBuffer.append(color.g);
        }
        else if (m_colorSequence == "BRG")
        {
            m_writeBuffer.append(color.b);
            m_writeBuffer.append(color.r);
            m_writeBuffer.append(color.g);
        }
        else if (m_colorSequence == "BGR")
        {
            m_writeBuffer.append(color.b);
            m_writeBuffer.append(color.g);
            m_writeBuffer.append(color.r);
        }
        else if (m_colorSequence == "GRB")
        {
            m_writeBuffer.append(color.g);
            m_writeBuffer.append(color.r);
            m_writeBuffer.append(color.b);
        }
        else if (m_colorSequence == "GBR")
        {
            m_writeBuffer.append(color.g);
            m_writeBuffer.append(color.b);
            m_writeBuffer.append(color.r);
        }
        else
        {
            m_writeBuffer.append(color.r);
            m_writeBuffer.append(color.g);
            m_writeBuffer.append(color.b);
        }
    }

    bool ok = writeBuffer(m_writeBuffer);

    emit commandCompleted(ok);
}

void LedDeviceAdalight::initColors(const QList<QRgb> & colors)
{
    currColors = colors;

    labCur = std::vector<StructLabF>(currColors.count());
    labTarget = std::vector<StructLabF>(currColors.count());
    xyzCur = std::vector<StructXyz>(currColors.count());
    xyzTarget = std::vector<StructXyz>(currColors.count());
    rgbCur = std::vector<StructRgb>(currColors.count());
    rgbTarget = std::vector<StructRgb>(currColors.count());

    for(int i = 0; i < currColors.size(); i++)
    {
        rgbCur[i].r = (unsigned)qRed(currColors[i]);
        rgbCur[i].g = (unsigned)qGreen(currColors[i]);
        rgbCur[i].b = (unsigned)qBlue(currColors[i]);
        PrismatikMath::RgbToXyz(rgbCur[i], xyzCur[i]);
        PrismatikMath::XyzToLab(xyzCur[i], labCur[i]);
    }
    step_sizes = std::vector<std::vector<double>>(currColors.count());
}

void LedDeviceAdalight::UpdateTargetColor(const QList<QRgb> & colors)
{
    // TODO: add as setting
    const int numTransitions = 15;  // number of intermediate colors

    targetColors = colors;
    for(int i = 0; i < targetColors.size(); i++)
    {
        rgbTarget[i].r = qRed(targetColors[i]);
        rgbTarget[i].g = qGreen(targetColors[i]);
        rgbTarget[i].b = qBlue(targetColors[i]);
        PrismatikMath::RgbToXyz(rgbTarget[i], xyzTarget[i]);
        PrismatikMath::XyzToLab(xyzTarget[i], labTarget[i]);
    }


    // calculate step sizes between currColors and targetColors
    for(int i = 0; i < targetColors.size(); i++)
    {
        step_sizes[i] =
        {
            abs((labTarget[i].l - labCur[i].l) / numTransitions),
            abs((labTarget[i].a - labCur[i].a) / numTransitions),
            abs((labTarget[i].b - labCur[i].b) / numTransitions)
        };
    }
}

void LedDeviceAdalight::setColors(const QList<QRgb> & colors)
{
    // TODO: add as setting
    const int intervalMs = 20;      // time between two intermediate colors

    if (!m_smoothTimer)
    {
        m_smoothTimer = new QTimer(this);
        // coarse timer (default) can vary strongly. We don't need to be precise but 20ms resulted in mostly 30+ ms with the coarse timer.
        m_smoothTimer->setTimerType(Qt::PreciseTimer);
        connect(m_smoothTimer, SIGNAL(timeout()), this, SLOT(updateSmoothColorsTick()));
        m_smoothTimer->setInterval(intervalMs);
    }
    if(!currColors.count() && colors.count())
    {
        initColors(colors);
        m_smoothTimer->start();
    }

    UpdateTargetColor(colors);
}

void LedDeviceAdalight::switchOffLeds()
{
    int count = m_colorsSaved.count();
    m_colorsSaved.clear();

    for (int i = 0; i < count; i++)
        m_colorsSaved << 0;

    m_writeBuffer.clear();
    m_writeBuffer.append(m_writeBufferHeader);

    for (int i = 0; i < count; i++) {
        m_writeBuffer.append((char)0)
                        .append((char)0)
                        .append((char)0);
    }

    bool ok = writeBuffer(m_writeBuffer);
    emit commandCompleted(ok);
}

void LedDeviceAdalight::setRefreshDelay(int /*value*/)
{
    emit commandCompleted(true);
}

void LedDeviceAdalight::setColorDepth(int /*value*/)
{
    emit commandCompleted(true);
}

void LedDeviceAdalight::setSmoothSlowdown(int /*value*/)
{
    emit commandCompleted(true);
}

void LedDeviceAdalight::setColorSequence(QString value)
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << value;

    m_colorSequence = value;
    setColors(m_colorsSaved);
}

void LedDeviceAdalight::requestFirmwareVersion()
{
    emit firmwareVersion("unknown (adalight device)");
    emit commandCompleted(true);
}

void LedDeviceAdalight::updateDeviceSettings()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO;

    AbstractLedDevice::updateDeviceSettings();
    setColorSequence(Settings::getColorSequence(SupportedDevices::DeviceTypeAdalight));
}

void LedDeviceAdalight::open()
{
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << sender();

//	m_gamma = Settings::getDeviceGamma();
//	m_brightness = Settings::getDeviceBrightness();

    if (m_AdalightDevice != NULL)
        m_AdalightDevice->close();
    else
        m_AdalightDevice = new QSerialPort();

    m_AdalightDevice->setPortName(m_portName);// Settings::getAdalightSerialPortName());

    m_AdalightDevice->open(QIODevice::WriteOnly | QIODevice::Unbuffered);
    bool ok = m_AdalightDevice->isOpen();

    // Ubuntu 10.04: on every second attempt to open the device leads to failure
    if (ok == false)
    {
        // Try one more time
        m_AdalightDevice->open(QIODevice::WriteOnly);
        ok = m_AdalightDevice->isOpen();
    }

    if (ok)
    {
        DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Serial device" << m_AdalightDevice->portName() << "open";

        ok = m_AdalightDevice->setBaudRate(m_baudRate);//Settings::getAdalightSerialPortBaudRate());
        if (ok)
        {
            ok = m_AdalightDevice->setDataBits(QSerialPort::Data8);
            if (ok)
            {
                DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Baud rate	:" << m_AdalightDevice->baudRate();
                DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Data bits	:" << m_AdalightDevice->dataBits();
                DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Parity		:" << m_AdalightDevice->parity();
                DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Stop bits	:" << m_AdalightDevice->stopBits();
                DEBUG_LOW_LEVEL << Q_FUNC_INFO << "Flow		:" << m_AdalightDevice->flowControl();
            } else {
                qWarning() << Q_FUNC_INFO << "Set data bits 8 fail";
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Set baud rate" << m_baudRate << "fail";
        }

    } else {
        qWarning() << Q_FUNC_INFO << "Serial device" << m_AdalightDevice->portName() << "open fail. " << m_AdalightDevice->errorString();
        DEBUG_OUT << Q_FUNC_INFO << "Available ports:";
        QList<QSerialPortInfo> availPorts = QSerialPortInfo::availablePorts();
        for(int i=0; i < availPorts.size(); i++) {
            DEBUG_OUT << Q_FUNC_INFO << availPorts[i].portName();
        }
    }

    emit openDeviceSuccess(ok);
}

bool LedDeviceAdalight::writeBuffer(const QByteArray & buff)
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO << "Hex:" << buff.toHex();

    if (m_AdalightDevice == NULL || m_AdalightDevice->isOpen() == false)
        return false;

    int bytesWritten = m_AdalightDevice->write(buff);

    if (bytesWritten != buff.count())
    {
        qWarning() << Q_FUNC_INFO << "bytesWritten != buff.count():" << bytesWritten << buff.count() << " " << m_AdalightDevice->errorString();
        return false;
    }

    return true;
}

void LedDeviceAdalight::resizeColorsBuffer(int buffSize)
{
    if (m_colorsBuffer.count() == buffSize)
        return;

    m_colorsBuffer.clear();

    if (buffSize > MaximumNumberOfLeds::Adalight)
    {
        qCritical() << Q_FUNC_INFO << "buffSize > MaximumNumberOfLeds::Adalight" << buffSize << ">" << MaximumNumberOfLeds::Adalight;

        buffSize = MaximumNumberOfLeds::Adalight;
    }

    for (int i = 0; i < buffSize; i++)
    {
        m_colorsBuffer << StructRgb();
    }

    reinitBufferHeader(buffSize);
}

void LedDeviceAdalight::reinitBufferHeader(int ledsCount)
{
    m_writeBufferHeader.clear();

    // Initialize buffer header
    int ledsCountHi = ((ledsCount - 1) >> 8) & 0xff;
    int ledsCountLo = (ledsCount	- 1) & 0xff;

    m_writeBufferHeader.append((char)'A');
    m_writeBufferHeader.append((char)'d');
    m_writeBufferHeader.append((char)'a');
    m_writeBufferHeader.append((char)ledsCountHi);
    m_writeBufferHeader.append((char)ledsCountLo);
    m_writeBufferHeader.append((char)(ledsCountHi ^ ledsCountLo ^ 0x55));
}
