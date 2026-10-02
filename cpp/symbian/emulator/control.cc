// SPDX-License-Identifier: GPL-3.0-or-later
#include "symbian/emulator/control.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSaveFile>
#include <QTimer>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cmath>
#include <drivers/input/common.h>
#include <drivers/itc.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <nlohmann/json.hpp>
#include <qt/state.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <sys/stat.h>
#include <unistd.h>

#include "absl/status/status.h"

namespace symbian::emulator {
namespace {

nlohmann::json Response(const absl::StatusOr<nlohmann::json>& result) {
  const auto& status = result.status();
  nlohmann::json response{
      {"schema", "symbian.emulator-control/v1"},
      {"status", nlohmann::json{{"code", static_cast<int>(status.code())},
                                {"message", std::string(status.message())}}}};
  if (result.ok()) {
    response["result"] = *result;
  }
  return response;
}

std::string StringField(const nlohmann::json& request, std::string_view key) {
  const auto entry = request.find(key);
  return entry != request.end() && entry->is_string()
             ? entry->get<std::string>()
             : std::string();
}

std::string EncodeResponse(const absl::StatusOr<nlohmann::json>& result) {
  // Process names originate in guest bytes. Diagnostics replace invalid UTF-8
  // instead of allowing a strict serializer to abort a no-exceptions process.
  return Response(result).dump(-1, ' ', false,
                               nlohmann::json::error_handler_t::replace) +
         '\n';
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
  std::string directory;
  std::string report_path;
  QLocalServer server;
  std::mutex exits_mutex;
  std::vector<ExitRecord> exits;
  std::size_t exit_callback = 0;
  bool callback_registered = false;

  Impl(eka2l1::desktop::emulator* emulator, std::string root,
       std::string report)
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
    if (QFileInfo::exists(QString::fromStdString(report_path)) ||
        QFileInfo(QString::fromStdString(report_path)).isSymLink()) {
      return absl::AlreadyExistsError(
          "Final status destination already exists");
    }
    const std::string data = EncodeResponse(Handle({{"operation", "status"}}));
    QSaveFile output(QString::fromStdString(report_path));
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(data.data(), static_cast<qint64>(data.size())) !=
            static_cast<qint64>(data.size()) ||
        !output.commit()) {
      return absl::InternalError("Cannot publish final emulator status");
    }
    qInfo("Symbian control: final status published");
    return absl::OkStatus();
  }

  absl::StatusOr<nlohmann::json> Handle(const nlohmann::json& request) {
    const std::string operation = StringField(request, "operation");
    if (operation == "status") {
      nlohmann::json records = nlohmann::json::array();
      std::lock_guard lock(exits_mutex);
      for (const auto& record : exits) {
        records.push_back(nlohmann::json{{"uid", record.uid},
                                         {"type", record.type},
                                         {"reason", record.reason},
                                         {"name", record.name}});
      }
      return nlohmann::json{{"process_exits", records}};
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
      const auto x_field = request.find("x");
      const auto y_field = request.find("y");
      const std::string action = StringField(request, "action");
      if (x_field == request.end() || y_field == request.end() ||
          !x_field->is_number() || !y_field->is_number()) {
        return absl::InvalidArgumentError(
            "Invalid logical pointer coordinates");
      }
      const double x = x_field->get<double>();
      const double y = y_field->get<double>();
      if (!std::isfinite(x) || !std::isfinite(y) || x != std::floor(x) ||
          y != std::floor(y) || x < 0 || y < 0 ||
          x >= screen->current_mode().size.x ||
          y >= screen->current_mode().size.y ||
          (action != "press" && action != "release")) {
        return absl::InvalidArgumentError(
            "Invalid logical pointer coordinates");
      }
      eka2l1::drivers::input_event event{};
      event.type_ = eka2l1::drivers::input_event_type::touch;
      event.mouse_.raw_screen_pos_ = true;
      event.mouse_.pos_x_ = static_cast<int>(x);
      event.mouse_.pos_y_ = static_cast<int>(y);
      event.mouse_.button_ = eka2l1::drivers::mouse_button_left;
      event.mouse_.action_ = action == "press"
                                 ? eka2l1::drivers::mouse_action_press
                                 : eka2l1::drivers::mouse_action_release;
      // The existing frontend path acquires the kernel lock internally when
      // delivering to a grabbed window. Do not recursively lock that mutex.
      kernel_lock.unlock();
      state->winserv->queue_input_from_driver(event);
      return nlohmann::json{{"queued", true}};
    }

    const std::string name = StringField(request, "name");
    if (name.empty() || name.size() > 64) {
      return absl::InvalidArgumentError("Capture needs a bounded basename");
    }
    for (const char character : name) {
      if (!((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' ||
            character == '_')) {
        return absl::InvalidArgumentError("Invalid capture basename");
      }
    }
    const std::string path =
        (std::filesystem::path(directory) / (name + ".png")).string();
    if (QFileInfo::exists(QString::fromStdString(path)) ||
        QFileInfo(QString::fromStdString(path)).isSymLink()) {
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
    QSaveFile output(QString::fromStdString(path));
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") ||
        !output.commit()) {
      return absl::InternalError("Cannot publish capture image");
    }
    return nlohmann::json{{"path", path},
                          {"width", width},
                          {"height", height},
                          {"logical_width", size.x},
                          {"logical_height", size.y},
                          {"display_scale", scale},
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
        const std::string data = socket->readLine(4097).toStdString();
        absl::StatusOr<nlohmann::json> result =
            absl::InvalidArgumentError("Invalid bounded JSON request");
        const auto document =
            nlohmann::json::parse(data, nullptr, /*allow_exceptions=*/false);
        if (data.ends_with('\n') && data.size() <= 4096 &&
            !document.is_discarded() && document.is_object()) {
          result = Handle(document);
        }
        const std::string response = EncodeResponse(result);
        socket->write(response.data(), static_cast<qint64>(response.size()));
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
  const std::string path(socket_path);
  QFileInfo info(QString::fromStdString(path));
  const std::string parent = info.dir().canonicalPath().toStdString();
  struct stat metadata{};
  if (!info.isAbsolute() || parent.empty() || path.size() > 100 ||
      ::stat(parent.c_str(), &metadata) != 0 || metadata.st_uid != ::getuid() ||
      (metadata.st_mode & 077) != 0) {
    return absl::InvalidArgumentError(
        "Socket requires an owned private directory");
  }
  if (!state || !state->symsys) {
    return absl::FailedPreconditionError("Emulator system unavailable");
  }
  const std::string report = path + ".status.json";
  if (QFileInfo::exists(QString::fromStdString(report)) ||
      QFileInfo(QString::fromStdString(report)).isSymLink()) {
    return absl::AlreadyExistsError("Final status destination already exists");
  }
  auto impl = std::make_unique<Impl>(state, parent, report);
  impl->server.setSocketOptions(QLocalServer::UserAccessOption);
  if (!impl->server.listen(QString::fromStdString(path))) {
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
