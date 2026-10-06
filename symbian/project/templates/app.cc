#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <absl/status/status.h>
#include <symbian/concurrency/event_executor.h>
#include <symbian/native_status.h>
#include <w32std.h>

namespace {

int FooterHeight(TSize size) {
  return size.iHeight >= 480 ? 64 : 32;
}

void Text(CWindowGc& gc, const char* text, int x, int y) {
  // This starter uses ASCII text. Convert to UTF-16 at the drawing API;
  // replace this adapter with a real UTF-8 decoder for translated content.
  TUint16 units[96];
  int length = 0;
  while (text[length] != '\0' && length < 96) {
    units[length] = static_cast<unsigned char>(text[length]);
    ++length;
  }
  gc.DrawText(TPtrC16(units, length), TPoint(x, y));
}

void Pair(std::string& text, int position, int value) {
  text[static_cast<std::size_t>(position)] =
      static_cast<char>('0' + value / 10);
  text[static_cast<std::size_t>(position + 1)] =
      static_cast<char>('0' + value % 10);
}

absl::Status LogTime(std::vector<std::string>& lines) {
  TTime now;
  now.HomeTime();
  const TDateTime date = now.DateTime();
  const int year = date.Year();
  if (year < 0 || year > 9999) {
    return absl::OutOfRangeError("Year does not fit the displayed date");
  }
  std::string text = "0000-00-00 00:00:00";
  Pair(text, 0, year / 100);
  Pair(text, 2, year % 100);
  Pair(text, 5, date.Month() + 1);
  Pair(text, 8, date.Day() + 1);
  Pair(text, 11, date.Hour());
  Pair(text, 14, date.Minute());
  Pair(text, 17, date.Second());
  if (lines.size() == 12) {
    lines.erase(lines.begin());
  }
  lines.push_back(std::move(text));
  return absl::OkStatus();
}

void Draw(CWindowGc& gc, CFont* font, TSize size,
          const std::vector<std::string>& lines, bool focused) {
  gc.SetPenStyle(CGraphicsContext::ENullPen);
  gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
  gc.SetBrushColor(TRgb(0x001c2430));
  gc.DrawRect(TRect(0, 0, size.iWidth, size.iHeight));
  gc.UseFont(font);
  gc.SetPenColor(TRgb(0x00ffffff));
  gc.SetPenStyle(CGraphicsContext::ESolidPen);
  const bool compact = size.iHeight < 480;
  const int footer = FooterHeight(size);
  const int first_line = compact ? 80 : 136;
  const int available = size.iHeight - footer - first_line;
  const int spacing = available / 12 < 28 ? available / 12 : 28;
  const int line_height = spacing < 12 ? 12 : spacing;
  const int capacity = available / line_height + 1;
  const int count = static_cast<int>(lines.size());
  const int start = count > capacity ? count - capacity : 0;
  Text(gc, "Hello world!", 16, compact ? 22 : 40);
  Text(gc,
       compact ? "Tap / Enter: local time"
               : "Tap to log the current local time.",
       16, compact ? 42 : 72);
  Text(gc, focused ? "Active" : "Paused", 16, compact ? 60 : 100);
  for (int i = start; i < count; ++i) {
    Text(gc, lines[static_cast<std::size_t>(i)].c_str(), 16,
         first_line + (i - start) * line_height);
  }
  gc.SetBrushColor(TRgb(0x00406080));
  gc.DrawRect(TRect(0, size.iHeight - footer, size.iWidth, size.iHeight));
  Text(gc, "Clear", 20, size.iHeight - (compact ? 10 : 24));
  Text(gc, "Exit", size.iWidth - 76, size.iHeight - (compact ? 10 : 24));
  gc.DiscardFont();
}

TInt RunWindow(RWsSession& session, CWsScreenDevice& screen, CWindowGc& gc) {
  const TSize size = screen.SizeInPixels();
  if (size.iWidth < 176 || size.iHeight < 208) {
    return KErrNotSupported;
  }
  CFont* font = nullptr;
  // Ask the server for its nearest default font; no vendor typeface required.
  _LIT(KTypeface, "");
  const TFontSpec specification(KTypeface, size.iHeight >= 480 ? 180 : 120);
  TInt result = screen.GetNearestFontToDesignHeightInTwips(font, specification);
  if (result != KErrNone) {
    return result;
  }
  RWindowGroup group(session);
  result = group.Construct(1, ETrue);
  if (result != KErrNone) {
    screen.ReleaseFont(font);
    return result;
  }
  RWindow window(session);
  result = window.Construct(group, 2);
  if (result != KErrNone) {
    group.Close();
    screen.ReleaseFont(font);
    return result;
  }
  std::vector<std::string> lines;
  symbian::concurrency::EventExecutor executor;
  std::vector<symbian::concurrency::Task> tasks;
  absl::Status dispatch_status;
  int due = 0;
  unsigned int generation = 0;
  const absl::Status opened = executor.Open();
  if (!opened.ok()) {
    window.Close();
    group.Close();
    screen.ReleaseFont(font);
    return symbian::NativeErrorFromStatus(opened);
  }
  auto cancel_tasks = [&] {
    ++generation;
    due = 0;
    for (const auto& task : tasks) {
      task.Cancel();
    }
  };
  auto schedule_time = [&] {
    auto task = executor.ScheduleAfter(absl::Milliseconds(1500));
    task.OnReady([&,
                  scheduled_generation = generation](const auto& completion) {
      if (completion.ok()) {
        const absl::Status queued =
            executor.DispatchToEvent([&, scheduled_generation] {
              if (scheduled_generation == generation) {
                ++due;
              }
            });
        if (!queued.ok()) {
          dispatch_status = queued;
        }
      } else if (completion.status().code() != absl::StatusCode::kCancelled) {
        dispatch_status = completion.status();
      }
    });
    tasks.push_back(std::move(task));
  };
  bool running = true;
  bool focused = true;
  group.SetOrdinalPosition(0);
  window.SetExtent(TPoint(0, 0), size);
  window.SetVisible(ETrue);
  window.Activate();
  TRequestStatus events;
  TRequestStatus redraws;
  session.EventReady(&events);
  session.RedrawReady(&redraws);
  window.Invalidate();
  session.Flush();
  auto log_current_time = [&]() {
    result = symbian::NativeErrorFromStatus(LogTime(lines));
    if (result != KErrNone) {
      running = false;
      return false;
    }
    window.Invalidate();
    return true;
  };
  while (running) {
    // This is the sole consumer of this thread's request semaphore. Inspect
    // Window Server statuses before parking alongside native timer requests.
    const absl::Status dispatched = executor.DispatchReady();
    if (!dispatched.ok() || !dispatch_status.ok()) {
      result = symbian::NativeErrorFromStatus(dispatched.ok() ? dispatch_status
                                                              : dispatched);
      break;
    }
    std::erase_if(tasks, [](const auto& task) { return task.IsReady(); });
    const int ready = std::exchange(due, 0);
    for (int i = 0; i < ready; ++i) {
      if (!log_current_time()) {
        break;
      }
    }
    if (!running) {
      break;
    }
    if (ready == 0 && events == KRequestPending && redraws == KRequestPending) {
      executor.Park();
    }
    if (events != KRequestPending) {
      if (events.Int() != KErrNone) {
        result = events.Int();
        break;
      }
      TWsEvent event;
      session.GetEvent(event);
      switch (event.Type()) {
        case EEventFocusLost:
          focused = false;
          window.Invalidate();
          break;
        case EEventFocusGained:
          focused = true;
          window.Invalidate();
          break;
        case EEventWindowClose:
          running = false;
          break;
        case EEventKey:
          if (event.Key()->iCode == EKeyEscape ||
              event.Key()->iCode == EKeyDevice1) {
            running = false;
          } else if (event.Key()->iCode == EKeyBackspace ||
                     event.Key()->iCode == EKeyDevice0) {
            cancel_tasks();
            lines.clear();
            window.Invalidate();
          } else if (focused && (event.Key()->iCode == EKeyEnter ||
                                 event.Key()->iCode == EKeyDevice3)) {
            if (log_current_time()) {
              schedule_time();
            }
          }
          break;
        case EEventPointer:
          if (focused && event.Handle() == 2 &&
              event.Pointer()->iType == TPointerEvent::EButton1Down) {
            const TPoint position = event.Pointer()->iPosition;
            if (position.iY >= size.iHeight - FooterHeight(size)) {
              if (position.iX >= size.iWidth / 2) {
                running = false;
              } else {
                cancel_tasks();
                lines.clear();
              }
            } else {
              if (log_current_time()) {
                schedule_time();
              }
            }
            window.Invalidate();
          }
          break;
        default:
          break;
      }
      if (running) {
        session.EventReady(&events);
      }
    }
    if (redraws != KRequestPending) {
      if (redraws.Int() != KErrNone) {
        result = redraws.Int();
        break;
      }
      TWsRedrawEvent redraw;
      session.GetRedraw(redraw);
      if (redraw.Handle() == 2) {
        window.BeginRedraw(redraw.Rect());
        gc.Activate(window);
        Draw(gc, font, size, lines, focused);
        gc.Deactivate();
        window.EndRedraw();
      }
      if (running) {
        session.RedrawReady(&redraws);
      }
    }
    session.Flush();
  }
  session.EventReadyCancel();
  session.RedrawReadyCancel();
  if (events == KRequestPending) {
    User::WaitForRequest(events);
  }
  if (redraws == KRequestPending) {
    User::WaitForRequest(redraws);
  }
  cancel_tasks();
  executor.Close();
  window.Close();
  group.Close();
  session.Flush();
  screen.ReleaseFont(font);
  return result;
}

}  // namespace

int main() {
  RWsSession session;
  TInt result = session.Connect();
  if (result != KErrNone) {
    return result;
  }
  {
    CWsScreenDevice screen(session);
    result = screen.Construct();
    if (result == KErrNone) {
      CWindowGc gc(&screen);
      result = gc.Construct();
      if (result == KErrNone) {
        result = RunWindow(session, screen, gc);
      }
    }
  }
  session.Close();
  return result;
}
