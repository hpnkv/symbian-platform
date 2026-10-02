#include <w32std.h>

#include "async_bridge.h"
#include "model.h"

namespace {

using gui_app::Layout;
using gui_app::Model;
using gui_app::Rect;

TRect NativeRect(const Rect& rectangle) {
  return TRect(rectangle.x, rectangle.y, rectangle.x + rectangle.width,
               rectangle.y + rectangle.height);
}

void Fill(CWindowGc& gc, const Rect& rectangle, TUint color) {
  gc.SetBrushColor(TRgb(color));
  gc.DrawRect(NativeRect(rectangle));
}

void DrawDigit(CWindowGc& gc, int digit, int x, int y, int scale) {
  // Seven-segment digits avoid a dependency on font selection/resources.
  const Rect bars[7] = {{.x = x + scale, .y = y, .width = 3 * scale, .height = scale},
                        {.x = x + 4 * scale, .y = y + scale, .width = scale, .height = 3 * scale},
                        {.x = x + 4 * scale, .y = y + 5 * scale, .width = scale, .height = 3 * scale},
                        {.x = x + scale, .y = y + 8 * scale, .width = 3 * scale, .height = scale},
                        {.x = x, .y = y + 5 * scale, .width = scale, .height = 3 * scale},
                        {.x = x, .y = y + scale, .width = scale, .height = 3 * scale},
                        {.x = x + scale, .y = y + 4 * scale, .width = 3 * scale, .height = scale}};
  for (int bar = 0; bar < 7; ++bar) {
    constexpr unsigned char kSegments[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                                             0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    if (kSegments[digit] & (1U << bar)) {
      Fill(gc, bars[bar], 0x00e8ebf2);
    }
  }
}

__attribute__((noinline)) void DrawGui(CWindowGc& gc, const Layout& layout,
                                       const Model& model, bool pulse) {
  gc.SetPenStyle(CGraphicsContext::ENullPen);
  gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
  Fill(gc, {.x = 0, .y = 0, .width = layout.width, .height = layout.height}, 0x001b1c25);
  Fill(gc, layout.increment, 0x004e9f76);
  Fill(gc, layout.reset, 0x00bf8a38);
  Fill(gc, layout.exit, 0x00ba5967);
  if (pulse) {
    // A delayed Task lights this marker without changing the counter model.
    Fill(gc, {.x = 12, .y = 12, .width = 16, .height = 16}, 0x004e9f76);
  }
  const int icon = gui_app::DividePositive(layout.increment.width, 4);
  const int middle =
      layout.increment.y + gui_app::DividePositive(layout.increment.height, 2);
  const Rect buttons[] = {layout.increment, layout.reset, layout.exit};
  for (const Rect button : buttons) {
    // A horizontal stroke denotes reset; increment adds a vertical stroke.
    Fill(gc, {button.x + icon, middle - 2, button.width - 2 * icon, 4},
         0x00ffffff);
  }
  Fill(gc,
       {.x = layout.increment.x +
             gui_app::DividePositive(layout.increment.width, 2) - 2,
        .y = middle - gui_app::DividePositive(icon, 2), .width = 4, .height = icon},
       0x00ffffff);
  // The exit control uses a boxed mark to distinguish it from reset.
  Fill(gc,
       {.x = layout.exit.x + icon, .y = middle - gui_app::DividePositive(icon, 2), .width = 4,
        .height = icon},
       0x00ffffff);
  Fill(gc,
       {.x = layout.exit.x + layout.exit.width - icon - 4,
        .y = middle - gui_app::DividePositive(icon, 2), .width = 4, .height = icon},
       0x00ffffff);
  const int scale = gui_app::DigitScale(layout);
  int value = model.count();
  int digits[4];
  for (int i = 3; i >= 0; --i) {
    const auto divided = gui_app::Divide(static_cast<unsigned int>(value), 10);
    digits[i] = static_cast<int>(divided.remainder);
    value = static_cast<int>(divided.quotient);
  }
  const int left = gui_app::DividePositive(layout.width - 23 * scale, 2);
  for (int i = 0; i < 4; ++i) {
    DrawDigit(gc, digits[i], left + i * 6 * scale,
              gui_app::DividePositive(layout.height, 4), scale);
  }
}

TInt RunWindow(RWsSession& session, const CWsScreenDevice& screen, CWindowGc& gc) {
  const TSize size = screen.SizeInPixels();
  if (size.iWidth < 120 || size.iHeight < 160 || size.iWidth > 8192 ||
      size.iHeight > 8192) {
    return KErrNotSupported;
  }
  RWindowGroup group(session);
  TInt result = group.Construct(1, ETrue);
  if (result != KErrNone) {
    return result;
  }
  RWindow window(session);
  result = window.Construct(group, 2);
  if (result != KErrNone) {
    group.Close();
    return result;
  }
  const Layout layout = gui_app::MakeLayout(size.iWidth, size.iHeight);
  Model model;
  GuiAsync* async = GuiAsyncCreate();
  if (async == nullptr) {
    window.Close();
    group.Close();
    return KErrNoMemory;
  }
  result = GuiAsyncOpen(async);
  if (result != KErrNone) {
    GuiAsyncDestroy(async);
    window.Close();
    group.Close();
    return result;
  }
  bool pulse = false;
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
  while (model.running()) {
    const int due = GuiAsyncDispatch(async);
    if (due < 0) {
      result = due;
      break;
    }
    if (due > 0) {
      pulse = true;
      window.Invalidate();
    }
    if (due == 0 && events == KRequestPending && redraws == KRequestPending) {
      GuiAsyncPark(async);
    }
    if (events != KRequestPending) {
      if (events.Int() != KErrNone) {
        result = events.Int();
        break;
      }
      TWsEvent event;
      session.GetEvent(event);
      if (event.Handle() == 2 && event.Type() == EEventPointer &&
          event.Pointer()->iType == TPointerEvent::EButton1Down) {
        const TPoint position = event.Pointer()->iPosition;
        if (model.Tap(layout, position.iX, position.iY)) {
          if (layout.increment.Contains(position.iX, position.iY)) {
            pulse = false;
            GuiAsyncSchedule(async);
          } else if (layout.reset.Contains(position.iX, position.iY) ||
                     layout.exit.Contains(position.iX, position.iY)) {
            pulse = false;
            GuiAsyncCancel(async);
          }
          window.Invalidate();
        }
      }
      if (model.running()) {
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
        DrawGui(gc, layout, model, pulse);
        gc.Deactivate();
        window.EndRedraw();
      }
      if (model.running()) {
        session.RedrawReady(&redraws);
      }
    }
    session.Flush();
  }
  session.EventReadyCancel();
  session.RedrawReadyCancel();
  // Cancellation completes requests before their stack statuses disappear.
  if (events == KRequestPending) {
    User::WaitForRequest(events);
  }
  if (redraws == KRequestPending) {
    User::WaitForRequest(redraws);
  }
  GuiAsyncDestroy(async);
  window.Close();
  group.Close();
  session.Flush();
  return result;
}

}  // namespace

extern "C" int GuiMain() {
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
