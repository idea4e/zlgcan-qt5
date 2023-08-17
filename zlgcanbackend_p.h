#ifndef ZLGCANBACKEND_P_H
#define ZLGCANBACKEND_P_H

#include "zlgcan/zlgcan.h"
#include "zlgcanbackend.h"

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>
#include <QVariant>

QT_BEGIN_NAMESPACE

class ZLGCanWriteNotifier;

class ZLGCanBackendPrivate
{
    Q_DECLARE_PUBLIC(ZLGCanBackend)

    Q_DISABLE_COPY(ZLGCanBackendPrivate)

    enum class ConfigureMethod
    {
        ZCAN_INITCAN,
        ZCAN_SETVALUE,
        IPROPERTY_SETVALUE,
    };
    enum class ConfigureSequence
    {
        BEFORE_INIT_CAN,
        BEFORE_START_CAN,
        AFTER_START_CAN,
    };

    struct ConfigurationItem
    {
        QCanBusDevice::ConfigurationKey key{QCanBusDevice::UserKey};
        bool configurable{false};
        ConfigureMethod method{ConfigureMethod::ZCAN_SETVALUE};
        ConfigureSequence sequence{ConfigureSequence::BEFORE_INIT_CAN};
        QVariant value{};
    };

    struct Device
    {
        QString type{};
        unsigned int id{0};
        bool fd{false};
        unsigned int channels{0};
        QHash<QCanBusDevice::ConfigurationKey, ConfigurationItem> configurations{};
        QSet<unsigned int> bitrate{};
        QSet<unsigned int> data_field_bitrate{};
    };

public:
    explicit ZLGCanBackendPrivate(ZLGCanBackend* q);
    ~ZLGCanBackendPrivate();

public:
    void setInterfaceName(const QString& interfaceName);
    bool setConfigurationParameter(int key, const QVariant& value);

    bool open();
    void close();

    void startWrite();
    void startRead();

    void resetController();

private:
    bool setConfigurations(ConfigureSequence sequence);
    QString systemErrorString(int* error_code = nullptr);

private:
    ZLGCanBackend* const q_ptr;
    static QMultiMap<unsigned long long, ZLGCanBackendPrivate*> d_ptrs;
    static QMutex d_ptrs_mutex;

    Device m_device;
    unsigned int m_index{0};
    unsigned int m_channel{0};
    bool m_fd{false};

    DEVICE_HANDLE m_deviceHandle{INVALID_DEVICE_HANDLE};
    CHANNEL_HANDLE m_channelHandle{INVALID_CHANNEL_HANDLE};
    // QThread m_workerThread;
    QMutex m_channelMutex;

    QTimer m_readTimer;
    QTimer m_writeTimer;
};

QT_END_NAMESPACE

#endif // ZLGCANBACKEND_P_H
