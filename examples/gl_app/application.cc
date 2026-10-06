#include "application.h"

#include <absl/base/nullability.h>
#include <e32keys.h>
#include <hal.h>
#include <w32std.h>

#include "renderer.h"

namespace gl_app {
namespace {

TInt RunWindow(RWsSession* absl_nonnull session,
               CWsScreenDevice* absl_nonnull screen,
               RWindow* absl_nonnull window) {
  Renderer renderer;
  TInt result = renderer.Open(window);
  if (result != KErrNone) {
    return result;
  }
  renderer.Resize(screen->SizeInPixels());
  RTimer timer;
  TInt tick_period = 0;
  result = HAL::Get(HALData::ENanoTickPeriod, tick_period);
  if (result != KErrNone || tick_period <= 0) {
    return KErrNotSupported;
  }
  result = timer.CreateLocal();
  if (result != KErrNone) {
    return result;
  }
  TRequestStatus events, redraws, frame;
  session->EventReady(&events);
  session->RedrawReady(&redraws);
  timer.After(frame, 33000);
  bool running = true, dirty = true;
  float angle = 0.65f;
  const TUint32 start = User::NTickCount();
  while (running) {
    if (events != KRequestPending) {
      if (events.Int() != KErrNone) {
        result = events.Int();
        break;
      }
      TWsEvent event;
      session->GetEvent(event);
      if (event.Handle() == 2 && event.Type() == EEventPointer) {
        if (renderer.HandlePointer(*event.Pointer())) {
          running = false;
        }
        dirty = true;
      } else if (event.Type() == EEventKey &&
                 event.Key()->iCode == EKeyEscape) {
        running = false;
      } else if (event.Type() == EEventScreenDeviceChanged) {
        TSize size = screen->SizeInPixels();
        if (size.iWidth < 160 || size.iHeight < 240) {
          result = KErrNotSupported;
          break;
        }
        window->SetExtent(TPoint(0, 0), size);
        renderer.Resize(size);
        dirty = true;
      }
      if (running) {
        session->EventReady(&events);
      }
    }
    if (redraws != KRequestPending) {
      if (redraws.Int() != KErrNone) {
        result = redraws.Int();
        break;
      }
      TWsRedrawEvent redraw;
      session->GetRedraw(redraw);
      if (redraw.Handle() == 2) {
        window->BeginRedraw(redraw.Rect());
        window->EndRedraw();
        dirty = true;
      }
      if (running) {
        session->RedrawReady(&redraws);
      }
    }
    if (frame != KRequestPending) {
      if (frame.Int() != KErrNone) {
        result = frame.Int();
        break;
      }
      // Monotonic ticks keep rotation independent of render speed and UTC
      // changes. Unsigned subtraction tolerates a counter wrap.
      angle = 0.65f + static_cast<float>(User::NTickCount() - start) *
                          tick_period * 0.00000065f;
      dirty = true;
      if (running) {
        timer.After(frame, 33000);
      }
    }
    if (!running) {
      break;
    }
    if (dirty) {
      result = renderer.Draw(angle);
      if (result != KErrNone) {
        break;
      }
      dirty = false;
    }
    session->Flush();
    // One native request wait drives Window Server events and the frame timer.
    if (events == KRequestPending && redraws == KRequestPending &&
        frame == KRequestPending) {
      User::WaitForAnyRequest();
    }
  }
  timer.Cancel();
  session->EventReadyCancel();
  session->RedrawReadyCancel();
  if (frame == KRequestPending) {
    User::WaitForRequest(frame);
  }
  if (events == KRequestPending) {
    User::WaitForRequest(events);
  }
  if (redraws == KRequestPending) {
    User::WaitForRequest(redraws);
  }
  timer.Close();
  return result;
}

}  // namespace

int RunApplication() {
  RWsSession session;
  TInt result = session.Connect();
  if (result != KErrNone) {
    return result;
  }
  {
    CWsScreenDevice screen(session);
    result = screen.Construct();
    if (result == KErrNone) {
      const TSize size = screen.SizeInPixels();
      if (size.iWidth < 160 || size.iHeight < 240) {
        result = KErrNotSupported;
      } else {
        RWindowGroup group(session);
        result = group.Construct(1, ETrue);
        if (result == KErrNone) {
          RWindow window(session);
          result = window.Construct(group, 2);
          if (result == KErrNone) {
            group.SetOrdinalPosition(0);
            window.SetExtent(TPoint(0, 0), size);
            window.SetVisible(ETrue);
            window.Activate();
            window.Invalidate();
            session.Flush();
            result = RunWindow(&session, &screen, &window);
            window.Close();
          }
          group.Close();
        }
      }
    }
  }
  session.Close();
  return result;
}

}  // namespace gl_app
