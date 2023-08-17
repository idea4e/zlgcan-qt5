#include "zlgcanbackend.h"

#include "zlgcanbackend_p.h"

#include <QFile>
#include <QLoggingCategory>
#include <QXmlStreamReader>

QT_BEGIN_NAMESPACE

// Q_DECLARE_LOGGING_CATEGORY(QT_CANBUS_PLUGINS_ZLGCAN)

// #ifndef LINK_LIBPCANBASIC
// Q_GLOBAL_STATIC(QLibrary, zlgLibrary)
// #endif

// bool ZLGCanBackend::canCreate(QString* errorReason)
// {
// #ifdef LINK_LIBPCANBASIC
//     return true;
// #else
//     static bool symbolsResolved = resolvePeakCanSymbols(pcanLibrary());
//     if(Q_UNLIKELY(!symbolsResolved))
//     {
//         qCCritical(QT_CANBUS_PLUGINS_ZLGCAN, "Cannot load library: %ls", qUtf16Printable(pcanLibrary()->errorString()));
//         *errorReason = pcanLibrary()->errorString();
//         return false;
//     }
//     return true;
// #endif
// }

QList<QCanBusDeviceInfo> ZLGCanBackend::interfaces()
{
    QList<QCanBusDeviceInfo> devices;

    QFile file(":devices.xml");
    if(file.open(QFile::ReadOnly))
    {
        QXmlStreamReader xml(&file);
        while(xml.readNextStartElement())
        {
            if(Q_UNLIKELY("Devices" != xml.name().toString()))
            {
                break;
            }
            while(xml.readNextStartElement())
            {
                if("Device" == xml.name().toString() && xml.attributes().hasAttribute("type"))
                {
                    auto type = xml.attributes().value("type").toString();
                    auto fd = ("TRUE" == xml.attributes().value("fd").toString().toUpper());
                    auto channels = xml.attributes().value("channels").toInt();
                    channels = channels ? channels : 1;
                    Q_UNUSED(channels);
                    devices.append(std::move(QCanBusDevice::createDeviceInfo(type, false, fd)));
                }
                xml.skipCurrentElement();
            }
        }
    }

    return devices;
}

ZLGCanBackend::ZLGCanBackend(const QString& interfaceName, QObject* parent): QCanBusDevice(parent), d_ptr(new ZLGCanBackendPrivate(this))
{
    Q_D(ZLGCanBackend);

    // this->moveToThread(&d->m_workerThread);

    connect(&d->m_readTimer, &QTimer::timeout, this, [=]() {
        d->startRead();
    });
    connect(&d->m_writeTimer, &QTimer::timeout, this, [=]() {
        d->startWrite();
    });
    d->setInterfaceName(interfaceName);

    std::function<void()> f = std::bind(&ZLGCanBackend::resetController, this);
    setResetControllerFunction(f);
    // std::function<CanBusStatus()> g = std::bind(&ZLGCanBackend::busStatus, this);
    // setCanBusStatusGetter(g);
}

ZLGCanBackend::~ZLGCanBackend()
{
    ZLGCanBackend::close();
    delete d_ptr;
}

QString ZLGCanBackend::interpretErrorFrame(const QCanBusFrame& frame)
{
    Q_D(ZLGCanBackend);
    Q_UNUSED(d);
    Q_UNUSED(frame);

    return QString();
}

void ZLGCanBackend::setConfigurationParameter(int key, const QVariant& value)
{
    Q_D(ZLGCanBackend);

    if(d->setConfigurationParameter(key, value))
    {
        QCanBusDevice::setConfigurationParameter(key, value);
    }
}

bool ZLGCanBackend::writeFrame(const QCanBusFrame& frame)
{
    Q_D(ZLGCanBackend);

    if(Q_UNLIKELY(QCanBusDevice::ConnectedState != state()))
    {
        return false;
    }

    if(Q_UNLIKELY(!frame.isValid()))
    {
        setError(tr("Cannot write invalid QCanBusFrame"), QCanBusDevice::WriteError);
        return false;
    }

    if(Q_UNLIKELY(QCanBusFrame::DataFrame != frame.frameType() && QCanBusFrame::RemoteRequestFrame != frame.frameType() && QCanBusFrame::ErrorFrame != frame.frameType()))
    {
        setError(tr("Unable to write a frame with unacceptable type"), QCanBusDevice::WriteError);
        return false;
    }

    // if(Q_UNLIKELY(frame.hasFlexibleDataRateFormat()))
    // {
    //     setError(tr("CAN FD frame format not supported."), QCanBusDevice::WriteError);
    //     return false;
    // }

    enqueueOutgoingFrame(frame);

    if(Q_LIKELY(!d->m_writeTimer.isActive()))
    {
        d->m_writeTimer.start();
    }

    return true;
}

void ZLGCanBackend::close()
{
    Q_D(ZLGCanBackend);

    d->close();
    // d->m_workerThread.quit();
    // d->m_workerThread.wait();

    setState(QCanBusDevice::UnconnectedState);
}

bool ZLGCanBackend::open()
{
    Q_D(ZLGCanBackend);

    // d->m_workerThread.start();

    if(Q_UNLIKELY(!d->open()))
    {
        ZLGCanBackend::close();
        return false;
    }

    setState(QCanBusDevice::ConnectedState);
    return true;
}

void ZLGCanBackend::resetController()
{
    Q_D(ZLGCanBackend);

    d->resetController();
}

// QCanBusDevice::CanBusStatus ZLGCanBackend::busStatus() const
// {
//     Q_D(ZLGCanBackend);
//     Q_UNUSED(d);

//     return QCanBusDevice::CanBusStatus::Good;
// }

QT_END_NAMESPACE
