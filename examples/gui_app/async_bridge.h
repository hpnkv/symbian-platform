#ifndef SYMBIAN_GUI_APP_ASYNC_BRIDGE_H_
#define SYMBIAN_GUI_APP_ASYNC_BRIDGE_H_

#include <absl/base/nullability.h>

// The original Window Server headers remain in app.cc. Modern C++ and the
// owned timer/Future implementation live on the other side of this boundary.
struct GuiAsync;

GuiAsync* absl_nullable GuiAsyncCreate();
int GuiAsyncOpen(GuiAsync* absl_nonnull async);
void GuiAsyncSchedule(GuiAsync* absl_nonnull async);
void GuiAsyncCancel(GuiAsync* absl_nonnull async);
int GuiAsyncDispatch(GuiAsync* absl_nullable async);
void GuiAsyncPark(GuiAsync* absl_nonnull async);
void GuiAsyncDestroy(GuiAsync* absl_nullable async);

#endif  // SYMBIAN_GUI_APP_ASYNC_BRIDGE_H_
