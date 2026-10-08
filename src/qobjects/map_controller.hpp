#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <filesystem>
#include <memory>

namespace mts::map {
class MapLoader;
class MapSnapshotService;
}
namespace mts::runtime {
class ThreadPool;
}

class MapController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(MapController)
    QML_UNCREATABLE("MapController is owned by ChatController")
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    MapController(mts::runtime::ThreadPool& pool,
                  std::filesystem::path mapConfigPath,
                  std::filesystem::path cacheDir,
                  QObject* parent = nullptr);
    ~MapController() override;

    [[nodiscard]] auto ready() const -> bool;
    [[nodiscard]] auto name() const -> QString;
    [[nodiscard]] auto status() const -> QString;
    [[nodiscard]] auto loader() const -> mts::map::MapLoader const*;
    [[nodiscard]] auto snapshotService() const -> mts::map::MapSnapshotService const*;

signals:
    void readyChanged();
    void nameChanged();
    void statusChanged();

private:
    mts::runtime::ThreadPool& m_pool;
    std::unique_ptr<mts::map::MapLoader> m_loader;
    std::unique_ptr<mts::map::MapSnapshotService> m_snapshots;
    bool m_ready = false;
    QString m_name;
    QString m_status;
};
