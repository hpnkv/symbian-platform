#ifndef SYMBIAN_EXAMPLES_CLASSIC_DEMO_UI_UI_H_
#define SYMBIAN_EXAMPLES_CLASSIC_DEMO_UI_UI_H_

#include <e32std.h>
#include <absl/base/nullability.h>

namespace classic_demo_ui {

using FeatureAction = TInt (* absl_nonnull)(void* absl_nullable context);

// Keeps a native window open while its Run and Close buttons handle input.
// The action runs only when the user presses Run; preview pixels are then shown.
TInt Show(const TDesC& title, const TDesC& action, const TDesC& evidence,
          const TDesC& result_caption, FeatureAction feature,
          void* absl_nullable context,
          const TUint32* absl_nullable preview = nullptr,
          TInt preview_width = 0, TInt preview_height = 0);

}  // namespace classic_demo_ui

#endif  // SYMBIAN_EXAMPLES_CLASSIC_DEMO_UI_UI_H_
