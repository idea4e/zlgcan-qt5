#include "main.h"

#include "ZLGCanBackend.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(QT_CANBUS_PLUGINS_ZLGCAN, "qt.canbus.plugins.zlgcan")

QList<QCanBusDeviceInfo> ZLGCanBusPlugin::availableDevices(QString* errorMessage) const
{
    Q_UNUSED(errorMessage)

    // if(Q_UNLIKELY(!ZLGCanBackend::canCreate(errorMessage)))
    // {
    //     return QList<QCanBusDeviceInfo>();
    // }

    return ZLGCanBackend::interfaces();
}

QCanBusDevice* ZLGCanBusPlugin::createDevice(const QString& interfaceName, QString* errorMessage) const
{
    Q_UNUSED(errorMessage)

    // QString errorReason;
    // if(!ZLGCanBackend::canCreate(&errorReason))
    // {
    //     qCWarning(QT_CANBUS_PLUGINS_ZLGCAN, "%ls", qUtf16Printable(errorReason));
    //     if(errorMessage)
    //     {
    //         *errorMessage = errorReason;
    //     }
    //     return nullptr;
    // }

    return new ZLGCanBackend(interfaceName);
}

QT_END_NAMESPACE
