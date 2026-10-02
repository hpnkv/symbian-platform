#ifndef SYMBIAN_GUI_APP_ASYNC_BRIDGE_H_
#define SYMBIAN_GUI_APP_ASYNC_BRIDGE_H_

// The original Window Server headers remain in app.cc. Modern C++ and the
// owned timer/Future implementation live on the other side of this boundary.
struct GuiAsync;

GuiAsync* GuiAsyncCreate();
int GuiAsyncOpen(GuiAsync* async);
void GuiAsyncSchedule(GuiAsync* async);
void GuiAsyncCancel(GuiAsync* async);
int GuiAsyncDispatch(GuiAsync* async);
void GuiAsyncPark(GuiAsync* async);
void GuiAsyncDestroy(GuiAsync* async);

#endif  // SYMBIAN_GUI_APP_ASYNC_BRIDGE_H_
