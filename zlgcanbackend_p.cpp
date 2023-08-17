#include "zlgcanbackend_p.h"

#include "zlgcan/zlgcan.h"
#include "zlgcanbackend.h"

#include <QFile>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QXmlStreamReader>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_CANBUS_PLUGINS_ZLGCAN)

QMultiMap<unsigned long long, ZLGCanBackendPrivate*> ZLGCanBackendPrivate::d_ptrs{};
QMutex ZLGCanBackendPrivate::d_ptrs_mutex;

ZLGCanBackendPrivate::ZLGCanBackendPrivate(ZLGCanBackend* q): q_ptr(q) {}

ZLGCanBackendPrivate::~ZLGCanBackendPrivate()
{
    close();
}

void ZLGCanBackendPrivate::setInterfaceName(const QString& interfaceName)
{
    QString deviceType{};
    QXmlStreamReader interfaceNameXml(interfaceName);
    deviceType = interfaceName.toUpper();
    m_index = 0;
    m_channel = 0;
    /*!
     * parsing interfaceName, like this
     * <?xml version="1.0" encoding="utf-8"?>
     * <Device type="ZCAN_USBCAN_E_U" index="0" channel="0" />
     */
    while(interfaceNameXml.readNextStartElement())
    {
        if("DEVICE" == interfaceNameXml.name().toString().toUpper())
        {
            deviceType = interfaceNameXml.attributes().value("type").toString().toUpper();
            m_index = interfaceNameXml.attributes().value("index").toUInt();
            m_channel = interfaceNameXml.attributes().value("channel").toUInt();
            break;
        }
    }

    QFile file(":devices.xml");
    if(deviceType.isEmpty() || !file.open(QFile::ReadOnly))
    {
        return;
    }
    QXmlStreamReader devicesXml(&file);

    auto readBitRate = [](QXmlStreamReader& xml, QSet<unsigned int>& bitRate) {
        while(xml.readNextStartElement())
        {
            if("VALUE" == xml.name().toString().toUpper())
            {
                auto value{xml.readElementText().toUInt()};
                if(value)
                {
                    bitRate << value;
                }
            }
        }
    };

    auto readKey = [](QXmlStreamReader& xml, ConfigurationItem& configuration) {
        configuration.configurable = ("TRUE" == xml.attributes().value("configurable").toString().toUpper());

        if("ZCAN_INITCAN" == xml.attributes().value("method").toString().toUpper())
        {
            configuration.method = ConfigureMethod::ZCAN_INITCAN;
        }
        else if("ZCAN_SETVALUE" == xml.attributes().value("method").toString().toUpper())
        {
            configuration.method = ConfigureMethod::ZCAN_SETVALUE;
        }
        else if("IPROPERTY_SETVALUE" == xml.attributes().value("method").toString().toUpper())
        {
            configuration.method = ConfigureMethod::IPROPERTY_SETVALUE;
        }

        if("BEFORE_INIT_CAN" == xml.attributes().value("sequence").toString().toUpper())
        {
            configuration.sequence = ConfigureSequence::BEFORE_INIT_CAN;
        }
        else if("BEFORE_START_CAN" == xml.attributes().value("sequence").toString().toUpper())
        {
            configuration.sequence = ConfigureSequence::BEFORE_START_CAN;
        }
        else if("AFTER_START_CAN" == xml.attributes().value("sequence").toString().toUpper())
        {
            configuration.sequence = ConfigureSequence::AFTER_START_CAN;
        }
    };

    auto readConfiguration = [&]() {
        while(devicesXml.readNextStartElement())
        {
            if(devicesXml.attributes().hasAttribute("configurable") && ("FALSE" == devicesXml.attributes().value("configurable").toString().toUpper()))
            {
                devicesXml.skipCurrentElement();
                continue;
            }

            auto keyName{devicesXml.name().toString().toUpper()};
            if("RAWFILTE" == keyName)
            {
                auto& configuration = m_device.configurations[QCanBusDevice::RawFilterKey];
                configuration.key = QCanBusDevice::RawFilterKey;
                readKey(devicesXml, configuration);
                devicesXml.skipCurrentElement();
            }
            else if("ERRORFILTER" == keyName)
            {
                auto& configuration = m_device.configurations[QCanBusDevice::ErrorFilterKey];
                configuration.key = QCanBusDevice::ErrorFilterKey;
                readKey(devicesXml, configuration);
                devicesXml.skipCurrentElement();
            }
            else if("LOOPBACK" == keyName)
            {
                auto& configuration = m_device.configurations[QCanBusDevice::LoopbackKey];
                configuration.key = QCanBusDevice::LoopbackKey;
                readKey(devicesXml, configuration);
                devicesXml.skipCurrentElement();
            }
            else if("RECEIVEOWN" == keyName)
            {
                auto& configuration = m_device.configurations[QCanBusDevice::ReceiveOwnKey];
                configuration.key = QCanBusDevice::ReceiveOwnKey;
                readKey(devicesXml, configuration);
                devicesXml.skipCurrentElement();
            }
            else if("BITRATE" == keyName)
            {
                auto& configuration = m_device.configurations[QCanBusDevice::BitRateKey];
                configuration.key = QCanBusDevice::BitRateKey;
                readKey(devicesXml, configuration);
                readBitRate(devicesXml, m_device.bitrate);
                // devicesXml.skipCurrentElement();
            }
            else if("CANFD" == keyName)
            {
                auto& configuration = m_device.configurations[QCanBusDevice::CanFdKey];
                configuration.key = QCanBusDevice::CanFdKey;
                readKey(devicesXml, configuration);
                devicesXml.skipCurrentElement();
            }
            else if("DATABITRATE" == keyName)
            {
                auto& configuration = m_device.configurations[QCanBusDevice::DataBitRateKey];
                configuration.key = QCanBusDevice::DataBitRateKey;
                readKey(devicesXml, configuration);
                readBitRate(devicesXml, m_device.data_field_bitrate);
                // devicesXml.skipCurrentElement();
            }
        }
    };
    auto readConfigurations = [&]() {
        while(devicesXml.readNextStartElement())
        {
            if("CONFIGURATIONS" == devicesXml.name().toString().toUpper())
            {
                readConfiguration();
            }
            else
            {
                devicesXml.skipCurrentElement();
            }
        }
    };
    auto readDevice = [&]() {
        while(devicesXml.readNextStartElement())
        {
            if(("DEVICE" == devicesXml.name().toString().toUpper()) && devicesXml.attributes().hasAttribute("type") && (deviceType.toUpper() == devicesXml.attributes().value("type").toString().toUpper()))
            {
                m_device.type = devicesXml.attributes().value("type").toString().toUpper();
                m_device.id = devicesXml.attributes().value("id").toUInt();
                m_device.fd = ("TRUE" == devicesXml.attributes().value("fd").toLatin1().toUpper());
                m_device.channels = devicesXml.attributes().value("channels").toUInt();
                readConfigurations();
            }
            else
            {
                devicesXml.skipCurrentElement();
            }
        }
    };
    auto readDevices = [&]() {
        while(devicesXml.readNextStartElement())
        {
            if("DEVICES" == devicesXml.name().toString().toUpper())
            {
                readDevice();
            }
            else
            {
                devicesXml.skipCurrentElement();
                // return;
            }
        }
    };

    readDevices();
}

bool ZLGCanBackendPrivate::setConfigurationParameter(int key, const QVariant& value)
{
    auto configurationKey = static_cast<QCanBusDevice::ConfigurationKey>(key);
    if(!m_device.configurations.contains(configurationKey))
    {
        return false;
    }
    auto& configuration = m_device.configurations[configurationKey];
    configuration.value = value;
    return true;
}

bool ZLGCanBackendPrivate::open()
{
    Q_Q(ZLGCanBackend);

    if(Q_UNLIKELY(!m_device.id || (m_channel >= m_device.channels)))
    {
        return false;
    }

    QMutexLocker locker(&m_channelMutex);

    bool newDeviceHandle{false};
    bool newChannelHandle{false};
    unsigned long long key{m_device.id};
    key = (key << 32) | m_index;
    auto range = d_ptrs.equal_range(key);
    for(auto iter = range.first; iter != range.second; ++iter)
    {
        m_deviceHandle = iter.value()->m_deviceHandle;
        if(Q_UNLIKELY(m_channel == iter.value()->m_channel && this != iter.value()))
        {
            m_deviceHandle = INVALID_DEVICE_HANDLE;
            goto FAILED;
        }
    }

    if(INVALID_DEVICE_HANDLE == m_deviceHandle)
    {
        m_deviceHandle = ::ZCAN_OpenDevice(m_device.id, m_index, 0);
        newDeviceHandle = true;
        if(Q_UNLIKELY(INVALID_DEVICE_HANDLE == m_deviceHandle))
        {
            goto FAILED;
        }
    }

    setConfigurations(ConfigureSequence::BEFORE_INIT_CAN);

    if(INVALID_CHANNEL_HANDLE == m_channelHandle)
    {
        ZCAN_CHANNEL_INIT_CONFIG config;
        ::memset(&config, 0, sizeof(config));
        config.can_type = m_fd ? 1 : 0;
        if(config.can_type)
        {
            config.canfd.acc_code = 0;
            config.canfd.acc_mask = 0xFFFFFFFF;
            // config.canfd.filter = 1;
            // config.canfd.mode = 0;
        }
        else
        {
            config.can.acc_code = 0;
            config.can.acc_mask = 0xFFFFFFFF;
            // config.can.filter = 1;
            // config.can.mode = 0;
        }
        m_channelHandle = ::ZCAN_InitCAN(m_deviceHandle, m_channel, &config);
        newChannelHandle = true;
        if(Q_UNLIKELY(INVALID_CHANNEL_HANDLE == m_channelHandle))
        {
            goto FAILED;
        }
    }

    setConfigurations(ConfigureSequence::BEFORE_START_CAN);

    if(Q_UNLIKELY(STATUS_OK != ::ZCAN_StartCAN(m_channelHandle)))
    {
        goto FAILED;
    }

    setConfigurations(ConfigureSequence::AFTER_START_CAN);

    if(newChannelHandle)
    {
        QMutexLocker locker(&d_ptrs_mutex);
        d_ptrs.insert(key, this);
    }

    m_readTimer.start();
    return true;

FAILED:
    if(newDeviceHandle && INVALID_DEVICE_HANDLE != m_deviceHandle)
    {
        ::ZCAN_CloseDevice(m_deviceHandle);
    }
    m_deviceHandle = INVALID_DEVICE_HANDLE;
    m_channelHandle = INVALID_CHANNEL_HANDLE;
    const auto errorString(systemErrorString());
    q->setError(errorString, QCanBusDevice::CanBusError::ConnectionError);
    return false;
}

void ZLGCanBackendPrivate::close()
{
    QMutexLocker channel_locker(&m_channelMutex);

    m_readTimer.stop();
    m_writeTimer.stop();

    unsigned long long key{m_device.id};
    key = (key << 32) | m_index;
    {
        QMutexLocker locker(&d_ptrs_mutex);
        d_ptrs.remove(key, this);
    }
    auto range = d_ptrs.equal_range(key);
    if(range.first == range.second && INVALID_DEVICE_HANDLE != m_deviceHandle)
    {
        ::ZCAN_CloseDevice(m_deviceHandle);
    }

    m_deviceHandle = INVALID_DEVICE_HANDLE;
    m_channelHandle = INVALID_CHANNEL_HANDLE;
}

constexpr unsigned int WRITE_BUFFER_SIZE{50};

void ZLGCanBackendPrivate::startWrite()
{
    Q_Q(ZLGCanBackend);

    if(Q_UNLIKELY(INVALID_CHANNEL_HANDLE == m_channelHandle || !q->hasOutgoingFrames()))
    {
        // QMetaObject::invokeMethod(&m_writeTimer, "stop", Qt::AutoConnection);
        m_writeTimer.stop();
        return;
    }

    QMutexLocker locker(&m_channelMutex);

    ZCAN_Transmit_Data data[WRITE_BUFFER_SIZE];
    ZCAN_TransmitFD_Data data_fd[WRITE_BUFFER_SIZE];

    auto writeFrame = [&]() {
        unsigned int num{0};
        constexpr auto BUFFER_SIZE = sizeof(data) / sizeof(data[0]);
        while(q->hasOutgoingFrames() && num < BUFFER_SIZE)
        {
            const QCanBusFrame frame = q->dequeueOutgoingFrame();
            if(Q_UNLIKELY(!frame.isValid()))
            {
                const QString errorString("Invalid frame.");
                qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.toLatin1());
                // q->setError(errorString, QCanBusDevice::WriteError);
                continue;
            }
            if(Q_UNLIKELY(frame.hasFlexibleDataRateFormat()))
            {
                const QString errorString("Cannot send CAN FD frame format as CAN FD is not enabled.");
                qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.toLatin1());
                q->setError(errorString, QCanBusDevice::WriteError);
                continue;
            }
            const QByteArray payload = frame.payload();
            if(Q_UNLIKELY(payload.size() > sizeof(ZCAN_Transmit_Data::frame.data) / sizeof(ZCAN_Transmit_Data::frame.data[0])))
            {
                QString errorString("Cannot write frame with payload size %1.");
                qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.arg(payload.size()).toLatin1());
                // q->setError(errorString, QCanBusDevice::WriteError);
                continue;
            }
            ::memset(&data[num], 0, sizeof(ZCAN_Transmit_Data));
            data[num].frame.can_id = MAKE_CAN_ID(frame.frameId(), frame.hasExtendedFrameFormat(), frame.frameType() == QCanBusFrame::RemoteRequestFrame, frame.frameType() == QCanBusFrame::ErrorFrame);
            data[num].frame.can_dlc = payload.size();
            ::memcpy_s(data[num].frame.data, sizeof(ZCAN_Transmit_Data::frame.data), payload, payload.size());
            ++num;
        }
        return ::ZCAN_Transmit(m_channelHandle, data, num);
    };

    auto writeFrameFD = [&]() {
        unsigned int num{0};
        constexpr auto BUFFER_SIZE = sizeof(data_fd) / sizeof(data_fd[0]);
        while(q->hasOutgoingFrames() && num < BUFFER_SIZE)
        {
            const QCanBusFrame frame = q->dequeueOutgoingFrame();
            if(Q_UNLIKELY(!frame.isValid()))
            {
                const QString errorString("Invalid frame.");
                qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.toLatin1());
                // q->setError(errorString, QCanBusDevice::WriteError);
                continue;
            }
            const QByteArray payload = frame.payload();
            if(Q_UNLIKELY(payload.size() > sizeof(ZCAN_TransmitFD_Data::frame.data) / sizeof(ZCAN_TransmitFD_Data::frame.data[0])))
            {
                QString errorString("Cannot write frame with payload size %1.");
                qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.arg(payload.size()).toLatin1());
                // q->setError(errorString, QCanBusDevice::WriteError);
                continue;
            }
            ::memset(&data_fd[num], 0, sizeof(ZCAN_TransmitFD_Data));
            data_fd[num].frame.can_id = MAKE_CAN_ID(frame.frameId(), frame.hasExtendedFrameFormat(), frame.frameType() == QCanBusFrame::RemoteRequestFrame, frame.frameType() == QCanBusFrame::ErrorFrame);
            data_fd[num].frame.flags |= frame.hasBitrateSwitch() ? CANFD_BRS : 0;
            data_fd[num].frame.len = payload.size();
            ::memcpy_s(data_fd[num].frame.data, sizeof(ZCAN_TransmitFD_Data::frame.data), payload, payload.size());
            ++num;
        }
        return ::ZCAN_TransmitFD(m_channelHandle, data_fd, num);
    };

    unsigned int writtenNum{0};
    while(q->hasOutgoingFrames())
    {
        auto ret = m_fd ? writeFrameFD() : writeFrame();
        if(ret > 0)
        {
            ++writtenNum;
        }
        else
        {
            const auto errorString(systemErrorString());
            qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.toLatin1());
            q->setError(errorString, QCanBusDevice::CanBusError::WriteError);
        }
    }
    if(writtenNum)
    {
        emit q->framesWritten(writtenNum);
    }
}

constexpr unsigned int READ_BUFFER_SIZE{50};

void ZLGCanBackendPrivate::startRead()
{
    Q_Q(ZLGCanBackend);

    if(Q_UNLIKELY(INVALID_CHANNEL_HANDLE == m_channelHandle))
    {
        // QMetaObject::invokeMethod(&m_readTimer, "quit", Qt::AutoConnection);
        m_readTimer.stop();
        return;
    }

    QMutexLocker locker(&m_channelMutex);

    QVector<QCanBusFrame> frames;
    frames.reserve(256);
    ZCAN_Receive_Data data[READ_BUFFER_SIZE];
    ZCAN_ReceiveFD_Data data_fd[READ_BUFFER_SIZE];
    QCanBusFrame frame;

    auto receiveFrame = [&](unsigned int size) {
        constexpr auto BUFFER_SIZE = sizeof(data) / sizeof(data[0]);
        size = size > BUFFER_SIZE ? BUFFER_SIZE : size;
        ::memset(&data, 0, sizeof(data[0]) * size);
        auto ret = ::ZCAN_Receive(m_channelHandle, data, size, 0);
        if(Q_UNLIKELY(ret < 0))
        {
            const auto errorString{systemErrorString()};
            qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.toLatin1());
            q->setError(errorString, QCanBusDevice::CanBusError::ReadError);
            return false;
        }
        for(decltype(ret) i = 0; i < ret; ++i)
        {
            frame.setFrameId(GET_ID(data[i].frame.can_id));
            frame.setPayload(QByteArray(reinterpret_cast<char*>(data[i].frame.data), int(data[i].frame.can_dlc)));
            frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(data[i].timestamp));
            frame.setExtendedFrameFormat(IS_EFF(data[i].frame.can_id));
            if(IS_ERR(data[i].frame.can_id))
            {
                frame.setFrameType(QCanBusFrame::ErrorFrame);
            }
            else if(IS_RTR(data[i].frame.can_id))
            {
                frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
            }
            else
            {
                frame.setFrameType(QCanBusFrame::DataFrame);
            }
            frames.append(frame);
        }
        return true;
    };

    auto receiveFrameFD = [&](unsigned int size) {
        constexpr auto BUFFER_SIZE = sizeof(data_fd) / sizeof(data_fd[0]);
        size = size > BUFFER_SIZE ? BUFFER_SIZE : size;
        ::memset(&data_fd, 0, sizeof(data_fd[0]) * size);
        auto ret = ::ZCAN_ReceiveFD(m_channelHandle, data_fd, size, 0);
        if(Q_UNLIKELY(ret < 0))
        {
            const auto errorString{systemErrorString()};
            qCWarning(QT_CANBUS_PLUGINS_ZLGCAN(), errorString.toLatin1());
            q->setError(errorString, QCanBusDevice::CanBusError::ReadError);
            return false;
        }
        for(decltype(ret) i = 0; i < ret; ++i)
        {
            frame.setFrameId(GET_ID(data_fd[i].frame.can_id));
            frame.setPayload(QByteArray(reinterpret_cast<char*>(data_fd[i].frame.data), int(data_fd[i].frame.len)));
            frame.setFlexibleDataRateFormat(true);
            frame.setBitrateSwitch(data_fd[i].frame.flags & CANFD_BRS);
            frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(data_fd[i].timestamp));
            frame.setExtendedFrameFormat(IS_EFF(data_fd[i].frame.can_id));
            if(IS_ERR(data_fd[i].frame.can_id))
            {
                frame.setFrameType(QCanBusFrame::ErrorFrame);
            }
            else
            {
                frame.setFrameType(QCanBusFrame::DataFrame);
            }
            frames.append(frame);
        }

        return true;
    };

    auto size{0U};
    while((size = ::ZCAN_GetReceiveNum(m_channelHandle, 0)))
    {
        receiveFrame(size);
        if(frames.size() >= 256)
        {
            q->enqueueReceivedFrames(frames);
            frames.clear();
        }
    }
    while(m_fd && (size = ::ZCAN_GetReceiveNum(m_channelHandle, 1)))
    {
        receiveFrameFD(size);
        if(frames.size() >= 256)
        {
            q->enqueueReceivedFrames(frames);
            frames.clear();
        }
    }
    if(!frames.isEmpty())
    {
        q->enqueueReceivedFrames(frames);
        frames.clear();
    }
}

void ZLGCanBackendPrivate::resetController()
{
    Q_Q(ZLGCanBackend);

    QMutexLocker locker(&m_channelMutex);

    if(Q_UNLIKELY(INVALID_CHANNEL_HANDLE == m_channelHandle))
    {
        return;
    }

    if(Q_UNLIKELY(STATUS_OK != ::ZCAN_ResetCAN(m_channelHandle) || STATUS_OK != ::ZCAN_StartCAN(m_channelHandle)))
    {
        const QString errorString = systemErrorString();
        qCWarning(QT_CANBUS_PLUGINS_ZLGCAN, "Cannot perform hardware reset: %ls", qUtf16Printable(errorString));
        q->setError(errorString, QCanBusDevice::CanBusError::ConfigurationError);
        close();
    }
    m_readTimer.start();
    m_writeTimer.start();
}

bool ZLGCanBackendPrivate::setConfigurations(ConfigureSequence sequence)
{
    Q_Q(ZLGCanBackend);
    Q_UNUSED(q)

    class IPropertyManager
    {
        Q_DISABLE_COPY(IPropertyManager)

    public:
        explicit IPropertyManager(void* handle): m_IProperty(::GetIProperty(handle)) {}

        virtual ~IPropertyManager()
        {
            if(m_IProperty)
            {
                ::ReleaseIProperty(m_IProperty);
            }
        }

        operator bool()
        {
            return m_IProperty;
        }

        IProperty* operator->()
        {
            return m_IProperty;
        }

    private:
        IProperty* m_IProperty;
    };

    if(INVALID_DEVICE_HANDLE == m_deviceHandle)
    {
        return false;
    }

    IPropertyManager IProperty(m_deviceHandle);

    auto setValue = [&](const QByteArray& path, const QByteArray& value, ConfigureMethod method) {
        if(Q_UNLIKELY(path.isEmpty() || value.isEmpty()))
        {
            return false;
        }
        if(ConfigureMethod::ZCAN_SETVALUE == method)
        {
            return (STATUS_OK == ::ZCAN_SetValue(m_deviceHandle, path, value));
        }
        else if(ConfigureMethod::IPROPERTY_SETVALUE == method)
        {
            return !!(IProperty->SetValue(path, value));
        }
        return true;
    };

    // set fd before all other configuration
    m_fd = false;
    auto iter = m_device.configurations.find(QCanBusDevice::CanFdKey);
    if(iter != m_device.configurations.end())
    {
        m_fd = m_device.fd && (iter->value.isValid() ? iter->value.toBool() : false);
    }

    QByteArray path, value;
    for(const auto& configuration: qAsConst(m_device.configurations))
    {
        if(!configuration.configurable || !configuration.value.isValid() || configuration.sequence != sequence)
        {
            continue;
        }

        path.clear();
        value.clear();
        switch(static_cast<int>(configuration.key))
        {
            case QCanBusDevice::RawFilterKey: break;
            case QCanBusDevice::ErrorFilterKey: break;
            case QCanBusDevice::LoopbackKey: break;
            case QCanBusDevice::ReceiveOwnKey: break;
            case QCanBusDevice::BitRateKey:
            {
                auto bitRate = configuration.value.isValid() ? configuration.value.toUInt() : 500000;
                QString str{QString("%1/baud_rate")};
                if(!m_device.bitrate.contains(bitRate))
                {
                    str = QString("%1/baud_rate_custom");
                }
                else if(m_fd)
                {
                    str = QString("%1/canfd_abit_baud_rate");
                }
                path = str.arg(m_channel).toLatin1();
                value = QByteArray::number(bitRate);
                break;
            }
            case QCanBusDevice::CanFdKey:
            {
                m_fd = m_device.fd && (configuration.value.isValid() ? configuration.value.toBool() : false);
                break;
            }
            case QCanBusDevice::DataBitRateKey:
            {
                auto dataBitRate = configuration.value.isValid() ? configuration.value.toUInt() : 500000;
                if(m_fd && m_device.data_field_bitrate.contains(dataBitRate))
                {
                    path = QString("%1/canfd_dbit_baud_rate").arg(m_channel).toLatin1();
                    value = QByteArray::number(dataBitRate);
                }
                break;
            }
        }
        if(!path.isEmpty() && !value.isEmpty() && !setValue(path, value, configuration.method))
        {
            return false;
        }
    }
    return true;
}

QString ZLGCanBackendPrivate::systemErrorString(int* errorCode)
{
    if(INVALID_CHANNEL_HANDLE == m_channelHandle)
    {
        return QString();
    }

    ZCAN_CHANNEL_ERR_INFO info;
    ::memset(&info, 0, sizeof(info));
    if(m_channelHandle && STATUS_OK == ::ZCAN_ReadChannelErrInfo(m_channelHandle, &info) && errorCode)
    {
        *errorCode = info.error_code;
    }
    QString errorString;
    switch(info.error_code)
    {
        case ZCAN_ERROR_CAN_OVERFLOW: errorString = "CAN controller FIFO overflow"; break;
        case ZCAN_ERROR_CAN_ERRALARM: errorString = "CAN controller alarm"; break;
        case ZCAN_ERROR_CAN_PASSIVE: errorString = "CAN controller passive"; break;
        case ZCAN_ERROR_CAN_LOSE: errorString = "CAN controller lose"; break;
        case ZCAN_ERROR_CAN_BUSERR: errorString = "CAN controller bus error"; break;
        case ZCAN_ERROR_CAN_BUSOFF: errorString = "CAN controller bus off"; break;
        case ZCAN_ERROR_CAN_BUFFER_OVERFLOW: errorString = "CAN buffer overflow"; break;
        case ZCAN_ERROR_DEVICEOPENED: errorString = "Device opened"; break;
        case ZCAN_ERROR_DEVICEOPEN: errorString = "Open device error"; break;
        case ZCAN_ERROR_DEVICENOTOPEN: errorString = "The buffer cannot be read"; break;
        case ZCAN_ERROR_BUFFEROVERFLOW: errorString = "Buffer overflow"; break;
        case ZCAN_ERROR_DEVICENOTEXIST: errorString = "Device not exist"; break;
        case ZCAN_ERROR_LOADKERNELDLL: errorString = "Load kernel dll error"; break;
        case ZCAN_ERROR_CMDFAILED: errorString = "Run command error"; break;
        case ZCAN_ERROR_BUFFERCREATE: errorString = "Create buffer error"; break;
        default: errorString = "Unknown error"; break;
    }
    return errorString;
}

QT_END_NAMESPACE
