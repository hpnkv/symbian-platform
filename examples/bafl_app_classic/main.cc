#include <bautils.h>
#include <s32mem.h>

#include "ui.h"

_LIT(KPublicFile, "Z:\\data\\animations\\startup.aac");

int RunFeature(void* absl_nullable) {
  RFs files;
  if (files.Connect() != KErrNone) {
    return 1;
  }
  TEntry entry;
  if (const TInt entry_error = files.Entry(KPublicFile, entry);
      entry_error != KErrNone) {
    files.Close();
    return 4;
  }
  const TBool found = BaflUtils::FileExists(files, KPublicFile);
  files.Close();
  if (!found) {
    return 2;
  }
  const TUint8 bytes[] = {1, 0, 0, 0};
  RMemReadStream stream(bytes, sizeof(bytes));
  TInt32 value = 0;
  TRAPD(error, value = stream.ReadInt32L());
  stream.Close();
  return error == KErrNone && value == 1 ? 0 : 3;
}

int main() {
  return classic_demo_ui::Show(_L("BAFL + STREAMS"), _L("Opens a public file."),
                               _L("Reads an integer from it."),
                               _L("INTEGER READ FROM FILE"), &RunFeature,
                               nullptr);
}
