#include <w32std.h>

#include "model.h"

namespace {

using gui_app::Layout;
using gui_app::Model;
using gui_app::Rect;

TRect NativeRect(Rect rectangle) {
  return TRect(rectangle.x, rectangle.y, rectangle.x + rectangle.width,
               rectangle.y + rectangle.height);
}

void Fill(CWindowGc& gc, Rect rectangle, TUint color) {
  gc.SetBrushColor(TRgb(color));
  gc.DrawRect(NativeRect(rectangle));
}

void DrawDigit(CWindowGc& gc, int digit, int x, int y, int scale) {
  // Seven-segment digits avoid a dependency on font selection/resources.
  constexpr unsigned char kSegments[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                                           0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  const Rect bars[7] = {{x + scale, y, 3 * scale, scale},
                        {x + 4 * scale, y + scale, scale, 3 * scale},
                        {x + 4 * scale, y + 5 * scale, scale, 3 * scale},
                        {x + scale, y + 8 * scale, 3 * scale, scale},
                        {x, y + 5 * scale, scale, 3 * scale},
                        {x, y + scale, scale, 3 * scale},
                        {x + scale, y + 4 * scale, 3 * scale, scale}};
  for (int bar = 0; bar < 7; ++bar) {
    if (kSegments[digit] & (1U << bar)) {
      Fill(gc, bars[bar], 0x00e8ebf2);
    }
  }
}

__attribute__((noinline)) void DrawGui(CWindowGc& gc, const Layout& layout,
                                       const Model& model) {
  gc.SetPenStyle(CGraphicsContext::ENullPen);
  gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
  Fill(gc, {0, 0, layout.width, layout.height}, 0x001b1c25);
  Fill(gc, layout.increment, 0x004e9f76);
  Fill(gc, layout.reset, 0x00bf8a38);
  Fill(gc, layout.exit, 0x00ba5967);
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
       {layout.increment.x +
            gui_app::DividePositive(layout.increment.width, 2) - 2,
        middle - gui_app::DividePositive(icon, 2), 4, icon},
       0x00ffffff);
  // The exit control uses a boxed mark to distinguish it from reset.
  Fill(gc,
       {layout.exit.x + icon, middle - gui_app::DividePositive(icon, 2), 4,
        icon},
       0x00ffffff);
  Fill(gc,
       {layout.exit.x + layout.exit.width - icon - 4,
        middle - gui_app::DividePositive(icon, 2), 4, icon},
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

TInt RunWindow(RWsSession& session, CWsScreenDevice& screen, CWindowGc& gc) {
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
    User::WaitForRequest(events, redraws);
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
        DrawGui(gc, layout, model);
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
