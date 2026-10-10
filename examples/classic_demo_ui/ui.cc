#include "ui.h"

#include <absl/base/nullability.h>
#include <gdi.h>
#include <w32std.h>

namespace classic_demo_ui {
namespace {

void Fill(CWindowGc* absl_nonnull gc, const TRect& rectangle, TUint color) {
  gc->SetPenStyle(CGraphicsContext::ENullPen);
  gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
  gc->SetBrushColor(
      TRgb((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff));
  gc->DrawRect(rectangle);
}

TInt Draw(CWsScreenDevice* absl_nonnull screen, CWindowGc* absl_nonnull gc,
          const TSize& size, const TDesC& title, const TDesC& action,
          const TDesC& evidence, const TDesC& result_caption, bool attempted,
          TInt feature_result, const TUint32* absl_nullable preview,
          TInt preview_width, TInt preview_height, const TRect& run_button,
          const TRect& close_button) {
  CFont* absl_nullable font = nullptr;
  const TFontSpec font_spec(_L("Series 60 Sans"), 20);
  if (const TInt result = screen->GetNearestFontInPixels(font, font_spec);
      result != KErrNone) {
    return result;
  }
  gc->UseFont(font);
  Fill(gc, TRect(TPoint(0, 0), size), 0x00151f2d);
  Fill(gc, TRect(12, 20, size.iWidth - 12, 68), 0x002e607c);
  Fill(gc, TRect(12, 86, size.iWidth - 12, 228), 0x00243143);
  Fill(gc, TRect(12, 244, size.iWidth - 12, 300),
       !attempted                   ? 0x003b526c
       : feature_result == KErrNone ? 0x002e7d62
                                    : 0x009d4b52);
  Fill(gc, run_button, 0x002e7d62);
  Fill(gc, close_button, 0x009d4b52);
  gc->SetPenStyle(CGraphicsContext::ESolidPen);
  gc->SetPenColor(TRgb(0x00ffffff));
  gc->DrawText(title, TPoint(24, 51));
  gc->DrawText(action, TPoint(24, 133));
  gc->DrawText(evidence, TPoint(24, 178));
  if (!attempted) {
    gc->DrawText(_L("TAP RUN TO TRY IT"), TPoint(24, 278));
  } else if (feature_result == KErrNone) {
    gc->DrawText(result_caption, TPoint(24, 278));
  } else {
    TBuf<32> status;
    status.Format(_L("FAILED: %d"), feature_result);
    gc->DrawText(status, TPoint(24, 278));
  }
  gc->DrawText(_L("RUN FEATURE"),
               TPoint(run_button.iTl.iX + 14, run_button.iTl.iY + 39));
  gc->DrawText(_L("CLOSE"),
               TPoint(close_button.iTl.iX + 32, close_button.iTl.iY + 39));
  if (attempted && feature_result == KErrNone && preview != nullptr &&
      preview_width > 0 && preview_width <= 16 && preview_height > 0 &&
      preview_height <= 16 && size.iHeight >= 560) {
    Fill(gc, TRect(12, 320, size.iWidth - 12, run_button.iTl.iY - 14),
         0x00243143);
    gc->SetPenStyle(CGraphicsContext::ESolidPen);
    gc->DrawText(_L("FEATURE OUTPUT"), TPoint(24, 350));
    const TInt longest =
        preview_width > preview_height ? preview_width : preview_height;
    const TInt pixel_size = 120 / longest;
    const TInt left = (size.iWidth - preview_width * pixel_size) / 2;
    for (TInt y = 0; y < preview_height; ++y) {
      for (TInt x = 0; x < preview_width; ++x) {
        Fill(gc,
             TRect(left + x * pixel_size, 370 + y * pixel_size,
                   left + (x + 1) * pixel_size, 370 + (y + 1) * pixel_size),
             preview[y * preview_width + x]);
      }
    }
  }
  gc->DiscardFont();
  screen->ReleaseFont(font);
  return KErrNone;
}

TInt Display(RWsSession* absl_nonnull session,
             CWsScreenDevice* absl_nonnull screen, CWindowGc* absl_nonnull gc,
             const TDesC& title, const TDesC& action, const TDesC& evidence,
             const TDesC& result_caption, FeatureAction feature,
             void* absl_nullable context, const TUint32* absl_nullable preview,
             TInt preview_width, TInt preview_height) {
  const TSize size = screen->SizeInPixels();
  if (size.iWidth < 300 || size.iHeight < 480) {
    return KErrNotSupported;
  }
  RWindowGroup group(*session);
  TInt result = group.Construct(1, ETrue);
  if (result != KErrNone) {
    return result;
  }
  RWindow window(*session);
  result = window.Construct(group, 2);
  if (result != KErrNone) {
    group.Close();
    return result;
  }
  group.SetOrdinalPosition(0);
  window.SetExtent(TPoint(0, 0), size);
  window.SetVisible(ETrue);
  window.Activate();
  const TRect run_button(12, size.iHeight - 88, size.iWidth / 2 - 8,
                         size.iHeight - 24);
  const TRect close_button(size.iWidth / 2 + 8, size.iHeight - 88,
                           size.iWidth - 12, size.iHeight - 24);
  bool running = true;
  bool attempted = false;
  TInt feature_result = KErrNone;
  TRequestStatus event_status;
  TRequestStatus redraw_status;
  session->EventReady(&event_status);
  session->RedrawReady(&redraw_status);
  window.Invalidate();
  session->Flush();
  while (running) {
    User::WaitForRequest(event_status, redraw_status);
    if (event_status != KRequestPending) {
      result = event_status.Int();
      if (result != KErrNone) {
        break;
      }
      TWsEvent event;
      session->GetEvent(event);
      if (event.Handle() == 2 && event.Type() == EEventPointer &&
          event.Pointer()->iType == TPointerEvent::EButton1Down) {
        if (const TPoint position = event.Pointer()->iPosition;
            close_button.Contains(position)) {
          running = false;
        } else if (run_button.Contains(position)) {
          attempted = true;
          feature_result = feature(context);
          window.Invalidate();
        }
      }
      if (running) {
        session->EventReady(&event_status);
      }
    }
    if (running && redraw_status != KRequestPending) {
      result = redraw_status.Int();
      if (result != KErrNone) {
        break;
      }
      TWsRedrawEvent redraw;
      session->GetRedraw(redraw);
      if (redraw.Handle() == 2) {
        window.BeginRedraw(redraw.Rect());
        gc->Activate(window);
        result = Draw(screen, gc, size, title, action, evidence, result_caption,
                      attempted, feature_result, preview, preview_width,
                      preview_height, run_button, close_button);
        gc->Deactivate();
        window.EndRedraw();
        if (result != KErrNone) {
          break;
        }
      }
      session->RedrawReady(&redraw_status);
    }
    session->Flush();
  }
  session->EventReadyCancel();
  session->RedrawReadyCancel();
  if (event_status == KRequestPending) {
    User::WaitForRequest(event_status);
  }
  if (redraw_status == KRequestPending) {
    User::WaitForRequest(redraw_status);
  }
  window.Close();
  group.Close();
  session->Flush();
  return result == KErrNone && attempted ? feature_result : result;
}

}  // namespace

TInt Show(const TDesC& title, const TDesC& action, const TDesC& evidence,
          const TDesC& result_caption, FeatureAction feature,
          void* absl_nullable context, const TUint32* absl_nullable preview,
          TInt preview_width, TInt preview_height) {
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
        result = Display(&session, &screen, &gc, title, action, evidence,
                         result_caption, feature, context, preview,
                         preview_width, preview_height);
      }
    }
  }
  session.Close();
  return result;
}

}  // namespace classic_demo_ui
