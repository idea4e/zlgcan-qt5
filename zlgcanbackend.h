#ifndef ZLGCANBACKEND_H
#define ZLGCANBACKEND_H

#include <QCanBusDevice>
#include <QCanBusDeviceInfo>
#include <QCanBusFrame>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE

class ZLGCanBackendPrivate;

class ZLGCanBackend: public QCanBusDevice
{
    Q_OBJECT

    Q_DECLARE_PRIVATE(ZLGCanBackend)

    Q_DISABLE_COPY(ZLGCanBackend)

public:
    explicit ZLGCanBackend(const QString& interfaceName, QObject* parent = nullptr);
    ~ZLGCanBackend();

    // static bool canCreate(QString* errorReason);
    static QList<QCanBusDeviceInfo> interfaces();

    virtual QString interpretErrorFrame(const QCanBusFrame& frame) override;
    virtual void setConfigurationParameter(int key, const QVariant& value) override;
    virtual bool writeFrame(const QCanBusFrame& frame) override;
    void resetController();
    // QCanBusDevice::CanBusStatus busStatus() const;

protected:
    virtual void close() override;
    virtual bool open() override;

private:
    ZLGCanBackendPrivate* const d_ptr{nullptr};
};

QT_END_NAMESPACE

#endif // ZLGCANBACKEND_H
