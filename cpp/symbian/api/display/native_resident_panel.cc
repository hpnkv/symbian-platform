// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// The Window Server session stays on its own guest thread.

#include "native_resident_panel.h"

#include <atomic>

#include <e32base.h>
#include <e32property.h>
#include <w32std.h>

namespace symbian::api::display {
namespace {

TInt NameResidentWindowGroup(RWindowGroup& group,
                             const NativeResidentPanelOptions& options) {
  // AppArc uses NUL-separated ready status, UID, and caption fields.
  TUint16 name_data[96] = {};
  TInt length = 0;
  name_data[length++] = '4';
  name_data[length++] = '0';
  name_data[length++] = 0;
  for (int shift = 28; shift >= 0; shift -= 4) {
    const unsigned digit = (options.app_uid >> shift) & 15;
    name_data[length++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
  }
  name_data[length++] = 0;
  for (const char* ch = options.caption; *ch != '\0'; ++ch) {
    if (length >= 95) {
      return KErrArgument;
    }
    name_data[length++] = static_cast<unsigned char>(*ch);
  }
  name_data[length++] = 0;
  TPtrC16 name(name_data, length);
  return group.SetName(name);
}

class RaisePanelProperty {
 public:
  RaisePanelProperty(TUid category, TUint key)
      : category_(category), key_(key) {}

  TInt Open() {
    TInt result = RProperty::Define(category_, key_, RProperty::EInt);
    if (result == KErrAlreadyExists) {
      result = KErrNone;  // Reuse a definition left by a previous process.
    } else if (result == KErrNone) {
      defined_ = true;
    }
    if (result != KErrNone) {
      return result;
    }
    result = property_.Attach(category_, key_);
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
      RProperty::Delete(category_, key_);
    }
  }

 private:
  TUid category_;
  TUint key_;
  RProperty property_;
  bool defined_ = false;
};

const unsigned char* Glyph(char letter) {
  static const unsigned char a[7] = {14, 17, 17, 31, 17, 17, 17};
  static const unsigned char b[7] = {30, 17, 17, 30, 17, 17, 30};
  static const unsigned char c[7] = {15, 16, 16, 16, 16, 16, 15};
  static const unsigned char d[7] = {30, 17, 17, 17, 17, 17, 30};
  static const unsigned char e[7] = {31, 16, 16, 30, 16, 16, 31};
  static const unsigned char f[7] = {31, 16, 16, 30, 16, 16, 16};
  static const unsigned char g[7] = {15, 16, 16, 23, 17, 17, 15};
  static const unsigned char h[7] = {17, 17, 17, 31, 17, 17, 17};
  static const unsigned char i[7] = {31, 4, 4, 4, 4, 4, 31};
  static const unsigned char k[7] = {17, 18, 20, 24, 20, 18, 17};
  static const unsigned char l[7] = {16, 16, 16, 16, 16, 16, 31};
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
    case 'D':
      return d;
    case 'E':
      return e;
    case 'F':
      return f;
    case 'G':
      return g;
    case 'H':
      return h;
    case 'I':
      return i;
    case 'K':
      return k;
    case 'L':
      return l;
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

const char* Heading(const NativeResidentPanelOptions& options) {
  const char* dynamic = options.heading_provider == nullptr
                            ? nullptr
                            : options.heading_provider();
  return dynamic == nullptr ? options.heading : dynamic;
}

void Draw(CWindowGc& gc, const TSize& size,
          const NativeResidentPanelOptions& options, const char* heading) {
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
  DrawLabel(gc, heading, 30, 30, 4);
  DrawLabel(gc, options.state, 40, 103, 3);
  DrawLabel(gc, options.back_label, 54, size.iHeight - 239, 4);
  DrawLabel(gc, options.stop_label, 54, size.iHeight - 139, 4);
}

TInt RunWindow(const NativeResidentPanelOptions& options,
               std::atomic<bool>& stop_requested) {
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
            result = NameResidentWindowGroup(group, options);
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
                RaisePanelProperty raise_panel(
                    TUid::Uid(options.property_category),
                    options.foreground_key);
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
                  const char* shown_heading = options.heading;
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
                        shown_heading = Heading(options);
                        Draw(gc, size, options, shown_heading);
                        gc.Deactivate();
                        window.EndRedraw();
                      }
                      if (!stop_requested.load()) {
                        session.RedrawReady(&redraws);
                      }
                    }
                    if (wake != KRequestPending && !stop_requested.load()) {
                      if (Heading(options) != shown_heading) {
                        window.Invalidate();
                      }
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

extern "C" int SymbianDeviceRunResidentPanel(
    const NativeResidentPanelOptions* options, void* stop_requested) {
  CTrapCleanup* cleanup = CTrapCleanup::New();
  if (cleanup == nullptr) {
    return KErrNoMemory;
  }
  const TInt result =
      RunWindow(*options, *static_cast<std::atomic<bool>*>(stop_requested));
  delete cleanup;
  return result;
}

extern "C" int SymbianDeviceRequestResidentPanelForeground(int category,
                                                           unsigned key) {
  RProperty property;
  TInt result = property.Attach(TUid::Uid(category), key);
  if (result != KErrNone) {
    return result;
  }
  TInt sequence = 0;
  result = property.Get(sequence);
  if (result == KErrNone) {
    result = property.Set(sequence == KMaxTInt ? 0 : sequence + 1);
  }
  property.Close();
  return result;
}

}  // namespace symbian::api::display
