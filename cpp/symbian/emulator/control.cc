// SPDX-License-Identifier: GPL-3.0-or-later
#include "symbian/emulator/control.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSaveFile>
#include <QTimer>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <cmath>
#include <drivers/input/common.h>
#include <drivers/itc.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <qt/state.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <sys/stat.h>
#include <unistd.h>

#include "absl/status/status.h"

namespace symbian::emulator {
namespace {

QJsonObject Response(const absl::StatusOr<QJsonObject>& result) {
  const auto& status = result.status();
  QJsonObject response{
      {"schema", "symbian.emulator-control/v1"},
      {"status", QJsonObject{{"code", static_cast<int>(status.code())},
                             {"message", QString::fromStdString(
                                             std::string(status.message()))}}}};
  if (result.ok()) {
    response.insert("result", *result);
  }
  return response;
}

}  // namespace

struct ControlServer::Impl {
  struct ExitRecord {
    uint32_t uid;
    int type;
    int reason;
    std::string name;
  };

  eka2l1::desktop::emulator* state;
  QString directory;
  QString report_path;
  QLocalServer server;
  std::mutex exits_mutex;
  std::vector<ExitRecord> exits;
  std::size_t exit_callback = 0;
  bool callback_registered = false;

  Impl(eka2l1::desktop::emulator* emulator, QString root, QString report)
      : state(emulator),
        directory(std::move(root)),
        report_path(std::move(report)) {}

  ~Impl() { Detach(); }

  void Detach() {
    server.close();
    if (callback_registered) {
      // The owner must detach before the OS worker destroys the kernel.
      auto* kernel = state->symsys->get_kernel_system();
      eka2l1::kernel_lock lock(kernel);
      kernel->unregister_process_exit_callback(exit_callback);
      callback_registered = false;
    }
  }

  absl::Status Stop() {
    qInfo("Symbian control: detaching kernel callbacks");
    Detach();
    qInfo("Symbian control: callbacks detached; draining replies");
    // application.exec() has returned, so queued replies no longer have an
    // event loop to flush them before the sockets are destroyed. Drain only
    // accepted output with a bounded wait; do not process new commands here.
    for (auto* socket : server.findChildren<QLocalSocket*>(
             QString(), Qt::FindDirectChildrenOnly)) {
      if (socket->bytesToWrite() > 0) {
        socket->waitForBytesWritten(100);
      }
    }
    if (QFileInfo::exists(report_path) || QFileInfo(report_path).isSymLink()) {
      return absl::AlreadyExistsError(
          "Final status destination already exists");
    }
    const auto data = QJsonDocument(Response(Handle({{"operation", "status"}})))
                          .toJson(QJsonDocument::Compact) +
                      '\n';
    QSaveFile output(report_path);
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(data) != data.size() || !output.commit()) {
      return absl::InternalError("Cannot publish final emulator status");
    }
    qInfo("Symbian control: final status published");
    return absl::OkStatus();
  }

  absl::StatusOr<QJsonObject> Handle(const QJsonObject& request) {
    const QString operation = request.value("operation").toString();
    if (operation == "status") {
      QJsonArray records;
      std::lock_guard lock(exits_mutex);
      for (const auto& record : exits) {
        records.append(
            QJsonObject{{"uid", static_cast<qint64>(record.uid)},
                        {"type", record.type},
                        {"reason", record.reason},
                        {"name", QString::fromStdString(record.name)}});
      }
      return QJsonObject{{"process_exits", records}};
    }
    if (operation != "capture" && operation != "pointer") {
      return absl::UnimplementedError("Unknown emulator control operation");
    }
    std::unique_lock state_lock(state->lockdown, std::try_to_lock);
    if (!state_lock.owns_lock() || !state->winserv || !state->graphics_driver) {
      return absl::UnavailableError(
          "Emulator graphics or services unavailable");
    }

    auto* kernel = state->symsys->get_kernel_system();
    std::unique_lock kernel_lock(*kernel, std::try_to_lock);
    if (!kernel_lock.owns_lock()) {
      return absl::UnavailableError("Emulator kernel busy");
    }
    auto* screen = state->winserv->get_current_focus_screen();
    if (!screen) {
      return absl::UnavailableError("No focused emulator screen");
    }
    if (operation == "pointer") {
      const auto x = request.value("x");
      const auto y = request.value("y");
      const auto action = request.value("action").toString();
      if (!x.isDouble() || !y.isDouble() || x.toDouble() != x.toInt(-1) ||
          y.toDouble() != y.toInt(-1) || x.toInt(-1) < 0 || y.toInt(-1) < 0 ||
          x.toInt() >= screen->current_mode().size.x ||
          y.toInt() >= screen->current_mode().size.y ||
          (action != "press" && action != "release")) {
        return absl::InvalidArgumentError(
            "Invalid logical pointer coordinates");
      }
      eka2l1::drivers::input_event event{};
      event.type_ = eka2l1::drivers::input_event_type::touch;
      event.mouse_.raw_screen_pos_ = true;
      event.mouse_.pos_x_ = x.toInt();
      event.mouse_.pos_y_ = y.toInt();
      event.mouse_.button_ = eka2l1::drivers::mouse_button_left;
      event.mouse_.action_ = action == "press"
                                 ? eka2l1::drivers::mouse_action_press
                                 : eka2l1::drivers::mouse_action_release;
      // The existing frontend path acquires the kernel lock internally when
      // delivering to a grabbed window. Do not recursively lock that mutex.
      kernel_lock.unlock();
      state->winserv->queue_input_from_driver(event);
      return QJsonObject{{"queued", true}};
    }

    const QString name = request.value("name").toString();
    if (name.isEmpty() || name.size() > 64) {
      return absl::InvalidArgumentError("Capture needs a bounded basename");
    }
    for (const QChar character : name) {
      if (!((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' ||
            character == '_')) {
        return absl::InvalidArgumentError("Invalid capture basename");
      }
    }
    const QString path = QDir(directory).filePath(name + ".png");
    if (QFileInfo::exists(path) || QFileInfo(path).isSymLink()) {
      return absl::AlreadyExistsError("Capture destination already exists");
    }
    eka2l1::vec2 size;
    float scale;
    eka2l1::drivers::handle texture;
    {
      std::lock_guard screen_lock(screen->screen_mutex);
      size = screen->current_mode().size;
      scale = screen->display_scale_factor;
      texture = screen->screen_texture;
    }
    // Never hold the kernel or screen lock while waiting for graphics work.
    kernel_lock.unlock();
    if (!std::isfinite(scale) || scale <= 0 || scale > 16 || size.x <= 0 ||
        size.y <= 0 || size.x * scale > 4096 || size.y * scale > 4096) {
      return absl::OutOfRangeError("Screen dimensions exceed capture bounds");
    }
    const int width = static_cast<int>(size.x * scale);
    const int height = static_cast<int>(size.y * scale);
    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
      return absl::ResourceExhaustedError("Cannot allocate capture image");
    }
    if (!eka2l1::drivers::read_bitmap(state->graphics_driver.get(), texture,
                                      {0, 0}, {width, height}, 32,
                                      image.bits())) {
      return absl::UnavailableError("Emulator screen texture read failed");
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") ||
        !output.commit()) {
      return absl::InternalError("Cannot publish capture image");
    }
    return QJsonObject{{"path", path},
                       {"width", width},
                       {"height", height},
                       {"source", "eka2l1-screen-texture"}};
  }

  void ConnectClient() {
    while (server.hasPendingConnections()) {
      auto* socket = server.nextPendingConnection();
      socket->setParent(&server);
      if (server
              .findChildren<QLocalSocket*>(QString(),
                                           Qt::FindDirectChildrenOnly)
              .size() > 16) {
        socket->abort();
        socket->deleteLater();
        continue;
      }
      socket->setReadBufferSize(4097);
      QTimer::singleShot(5000, socket, [socket] { socket->abort(); });
      QObject::connect(socket, &QLocalSocket::disconnected, socket,
                       &QObject::deleteLater);
      const auto read = [this, socket] {
        if (socket->property("handled").toBool()) {
          return;
        }
        if (!socket->canReadLine() && socket->bytesAvailable() < 4097) {
          return;
        }
        socket->setProperty("handled", true);
        const auto data = socket->readLine(4097);
        absl::StatusOr<QJsonObject> result =
            absl::InvalidArgumentError("Invalid bounded JSON request");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(data, &error);
        if (data.endsWith('\n') && data.size() <= 4096 &&
            error.error == QJsonParseError::NoError && document.isObject()) {
          result = Handle(document.object());
        }
        socket->write(
            QJsonDocument(Response(result)).toJson(QJsonDocument::Compact) +
            '\n');
        socket->disconnectFromServer();
      };
      QObject::connect(socket, &QLocalSocket::readyRead, socket, read);
      read();
    }
  }
};

ControlServer::ControlServer(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

ControlServer::~ControlServer() = default;

absl::Status ControlServer::Stop() {
  return impl_->Stop();
}

absl::StatusOr<std::unique_ptr<ControlServer>> ControlServer::Start(
    eka2l1::desktop::emulator* state, const char* socket_path) {
  if (!socket_path || !socket_path[0]) {
    return std::unique_ptr<ControlServer>();
  }
  const QString path = QString::fromUtf8(socket_path);
  QFileInfo info(path);
  const QString parent = info.dir().canonicalPath();
  struct stat metadata{};
  if (!info.isAbsolute() || parent.isEmpty() || path.toUtf8().size() > 100 ||
      ::stat(parent.toUtf8().constData(), &metadata) != 0 ||
      metadata.st_uid != ::getuid() || (metadata.st_mode & 077) != 0) {
    return absl::InvalidArgumentError(
        "Socket requires an owned private directory");
  }
  if (!state || !state->symsys) {
    return absl::FailedPreconditionError("Emulator system unavailable");
  }
  const QString report = path + ".status.json";
  if (QFileInfo::exists(report) || QFileInfo(report).isSymLink()) {
    return absl::AlreadyExistsError("Final status destination already exists");
  }
  auto impl = std::make_unique<Impl>(state, parent, report);
  impl->server.setSocketOptions(QLocalServer::UserAccessOption);
  if (!impl->server.listen(path)) {
    return absl::AlreadyExistsError("Cannot bind private control socket");
  }
  QObject::connect(&impl->server, &QLocalServer::newConnection, &impl->server,
                   [pointer = impl.get()] { pointer->ConnectClient(); });
  auto* kernel = state->symsys->get_kernel_system();
  {
    eka2l1::kernel_lock lock(kernel);
    impl->exit_callback = kernel->register_process_exit_callback(
        [pointer = impl.get()](eka2l1::kernel::process* process) {
          std::lock_guard guard(pointer->exits_mutex);
          if (pointer->exits.size() < 256) {
            pointer->exits.push_back(
                {process->get_uid(), static_cast<int>(process->get_exit_type()),
                 process->get_exit_reason(), process->name()});
          }
        });
    impl->callback_registered = true;
  }
  return std::unique_ptr<ControlServer>(new ControlServer(std::move(impl)));
}

}  // namespace symbian::emulator
