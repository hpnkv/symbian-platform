// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// The Window Server session stays on its own guest thread.

#include <atomic>

#include <e32base.h>
#include <e32property.h>
#include <w32std.h>

#include "agent_signals.h"

namespace {

TInt NameAgentWindowGroup(RWindowGroup& group) {
  // AppArc's APGWGNAM.CPP encodes status, UID, caption and document as
  // NUL-separated UTF-16 fields. Status 0x40 marks the application ready.
  static const TUint16 kName[] = {
      '4', '0', 0,   'e', '0', '0', '0', '0', 'a', '3', '1', 0,   'D', 'e', 'v',
      'e', 'l', 'o', 'p', 'm', 'e', 'n', 't', ' ', 'A', 'g', 'e', 'n', 't', 0};
  TPtrC16 name(kName, sizeof(kName) / sizeof(kName[0]));
  return group.SetName(name);
}

class RaisePanelProperty {
 public:
  TInt Open() {
    TInt result =
        RProperty::Define(agent_service::kPropertyCategory,
                          agent_service::kRaisePanelKey, RProperty::EInt);
    if (result == KErrAlreadyExists) {
      // A previous process can leave its definition behind after a crash.
      result = RProperty::Delete(agent_service::kPropertyCategory,
                                 agent_service::kRaisePanelKey);
      if (result == KErrNone) {
        result =
            RProperty::Define(agent_service::kPropertyCategory,
                              agent_service::kRaisePanelKey, RProperty::EInt);
      }
    }
    if (result != KErrNone) {
      return result;
    }
    defined_ = true;
    result = property_.Attach(agent_service::kPropertyCategory,
                              agent_service::kRaisePanelKey);
    if (result != KErrNone) {
      return result;
    }
    return property_.Set(0);
  }

  void Subscribe(TRequestStatus& status) { property_.Subscribe(status); }

  void Cancel() { property_.Cancel(); }

  ~RaisePanelProperty() {
    property_.Close();
    if (defined_) {
      RProperty::Delete(agent_service::kPropertyCategory,
                        agent_service::kRaisePanelKey);
    }
  }

 private:
  RProperty property_;
  bool defined_ = false;
};

const unsigned char* Glyph(char letter) {
  static const unsigned char a[7] = {14, 17, 17, 31, 17, 17, 17};
  static const unsigned char b[7] = {30, 17, 17, 30, 17, 17, 30};
  static const unsigned char c[7] = {15, 16, 16, 16, 16, 16, 15};
  static const unsigned char e[7] = {31, 16, 16, 30, 16, 16, 31};
  static const unsigned char g[7] = {15, 16, 16, 23, 17, 17, 15};
  static const unsigned char i[7] = {31, 4, 4, 4, 4, 4, 31};
  static const unsigned char k[7] = {17, 18, 20, 24, 20, 18, 17};
  static const unsigned char n[7] = {17, 25, 21, 19, 17, 17, 17};
  static const unsigned char o[7] = {14, 17, 17, 17, 17, 17, 14};
  static const unsigned char p[7] = {30, 17, 17, 30, 16, 16, 16};
  static const unsigned char r[7] = {30, 17, 17, 30, 20, 18, 17};
  static const unsigned char s[7] = {15, 16, 16, 14, 1, 1, 30};
  static const unsigned char t[7] = {31, 4, 4, 4, 4, 4, 4};
  static const unsigned char u[7] = {17, 17, 17, 17, 17, 17, 14};
  switch (letter) {
    case 'A':
      return a;
    case 'B':
      return b;
    case 'C':
      return c;
    case 'E':
      return e;
    case 'G':
      return g;
    case 'I':
      return i;
    case 'K':
      return k;
    case 'N':
      return n;
    case 'O':
      return o;
    case 'P':
      return p;
    case 'R':
      return r;
    case 'S':
      return s;
    case 'T':
      return t;
    case 'U':
      return u;
    default:
      return nullptr;
  }
}

void DrawLabel(CWindowGc& gc, const char* label, TInt x, TInt y, TInt scale) {
  for (TInt index = 0; label[index] != '\0'; ++index) {
    const unsigned char* glyph = Glyph(label[index]);
    if (glyph == nullptr) {
      continue;
    }
    for (TInt row = 0; row < 7; ++row) {
      for (TInt column = 0; column < 5; ++column) {
        if ((glyph[row] & (1 << (4 - column))) == 0) {
          continue;
        }
        const TInt left = x + (index * 6 + column) * scale;
        const TInt top = y + row * scale;
        gc.DrawRect(TRect(left, top, left + scale, top + scale));
      }
    }
  }
}

void Draw(CWindowGc& gc, const TSize& size) {
  gc.SetPenStyle(CGraphicsContext::ENullPen);
  gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
  gc.SetBrushColor(TRgb(0x00192332));
  gc.DrawRect(TRect(0, 0, size.iWidth, size.iHeight));
  gc.SetBrushColor(TRgb(0x00318f71));
  gc.DrawRect(TRect(24, 70, size.iWidth - 24, 170));
  gc.SetBrushColor(TRgb(0x00523c9f));
  gc.DrawRect(
      TRect(24, size.iHeight - 260, size.iWidth - 24, size.iHeight - 180));
  gc.SetBrushColor(TRgb(0x00ad4654));
  gc.DrawRect(
      TRect(24, size.iHeight - 160, size.iWidth - 24, size.iHeight - 80));
  gc.SetBrushColor(TRgb(0x00ffffff));
  DrawLabel(gc, "AGENT", 30, 30, 4);
  DrawLabel(gc, "RUNNING", 40, 103, 3);
  DrawLabel(gc, "BACK", 54, size.iHeight - 239, 4);
  DrawLabel(gc, "STOP", 54, size.iHeight - 139, 4);
}

TInt RunWindow(std::atomic<bool>& stop_requested) {
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
        const TSize size = screen.SizeInPixels();
        if (size.iWidth < 160 || size.iHeight < 240 || size.iWidth > 8192 ||
            size.iHeight > 8192) {
          result = KErrNotSupported;
        } else {
          RWindowGroup group(session);
          result = group.Construct(1, ETrue);
          if (result == KErrNone) {
            // A bare RWindowGroup is invisible to AppArc task lookup.
            result = NameAgentWindowGroup(group);
            if (result != KErrNone) {
              group.Close();
            }
          }
          if (result == KErrNone) {
            RWindow window(session);
            result = window.Construct(group, 2);
            if (result == KErrNone) {
              group.SetOrdinalPosition(0);
              window.SetExtent(TPoint(0, 0), size);
              window.SetVisible(ETrue);
              window.Activate();
              TRequestStatus events;
              TRequestStatus redraws;
              RTimer wake_timer;
              result = wake_timer.CreateLocal();
              if (result == KErrNone) {
                RaisePanelProperty raise_panel;
                result = raise_panel.Open();
                if (result == KErrNone) {
                  TRequestStatus wake;
                  TRequestStatus raise;
                  session.EventReady(&events);
                  session.RedrawReady(&redraws);
                  raise_panel.Subscribe(raise);
                  wake_timer.After(wake, 5000000);
                  window.Invalidate();
                  session.Flush();
                  while (!stop_requested.load()) {
                    User::WaitForAnyRequest();
                    if (events != KRequestPending) {
                      if (events.Int() != KErrNone) {
                        result = events.Int();
                        break;
                      }
                      TWsEvent event;
                      session.GetEvent(event);
                      if (event.Handle() == 2 &&
                          event.Type() == EEventPointer &&
                          event.Pointer()->iType ==
                              TPointerEvent::EButton1Down) {
                        const TPoint point = event.Pointer()->iPosition;
                        if (point.iX >= 24 && point.iX < size.iWidth - 24 &&
                            point.iY >= size.iHeight - 160 &&
                            point.iY < size.iHeight - 80) {
                          stop_requested.store(true);
                        } else if (point.iX >= 24 &&
                                   point.iX < size.iWidth - 24 &&
                                   point.iY >= size.iHeight - 260 &&
                                   point.iY < size.iHeight - 180) {
                          group.SetOrdinalPosition(-1);
                        }
                      }
                      if (!stop_requested.load()) {
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
                        Draw(gc, size);
                        gc.Deactivate();
                        window.EndRedraw();
                      }
                      if (!stop_requested.load()) {
                        session.RedrawReady(&redraws);
                      }
                    }
                    if (wake != KRequestPending && !stop_requested.load()) {
                      wake_timer.After(wake, 5000000);
                    }
                    if (raise != KRequestPending) {
                      if (raise.Int() != KErrNone) {
                        result = raise.Int();
                        break;
                      }
                      group.SetOrdinalPosition(0);
                      raise_panel.Subscribe(raise);
                    }
                    session.Flush();
                  }
                  session.EventReadyCancel();
                  session.RedrawReadyCancel();
                  wake_timer.Cancel();
                  raise_panel.Cancel();
                  if (events == KRequestPending) {
                    User::WaitForRequest(events);
                  }
                  if (redraws == KRequestPending) {
                    User::WaitForRequest(redraws);
                  }
                  if (wake == KRequestPending) {
                    User::WaitForRequest(wake);
                  }
                  if (raise == KRequestPending) {
                    User::WaitForRequest(raise);
                  }
                }
                wake_timer.Close();
              }
              window.Close();
            }
            group.Close();
          }
        }
      }
    }
  }
  session.Close();
  return result;
}

}  // namespace

extern "C" int AgentLocalUiMain(std::atomic<bool>* stop_requested) {
  CTrapCleanup* cleanup = CTrapCleanup::New();
  if (cleanup == nullptr) {
    return KErrNoMemory;
  }
  const TInt result = RunWindow(*stop_requested);
  delete cleanup;
  return result;
}

extern "C" int AgentRequestForeground() {
  RProperty property;
  TInt result = property.Attach(agent_service::kPropertyCategory,
                                agent_service::kRaisePanelKey);
  if (result != KErrNone) {
    return result;
  }
  TInt sequence = 0;
  result = property.Get(sequence);
  if (result == KErrNone) {
    result = property.Set(sequence + 1);
  }
  property.Close();
  return result;
}
