# Failure Handler Demo

Open the app to log short, long, formatted, multiline and Unicode messages,
then deliberately fail `CHECK_EQ(2 + 2, 5)`. The SDK automatically saves the
report to `C:\private\e0000a72\failure.txt` and shows its failure view.

Drag the text to check scrolling and wrapping. Copy Logs copies the complete
report, including text outside the viewport. Exit immediately destroys the
failure window. A separate SDK worker saves and flushes the report independently
of presentation; if saving is still in progress, the process remains alive
without that window until it finishes. Presentation failure does not cancel
saving, and saving failure does not close the view.
The example uses public SDK facilities and Abseil; it needs no device-specific
configuration or additional capabilities.

With `SYMBIAN_SDK_PREFIX` set to an installed SDK:

```sh
symbian build --project examples/failure_handler_app
symbian package --project examples/failure_handler_app \
  --artifact .symbian/build/failure_handler_app.exe \
  --signing-certificate /path/to/signing.cer \
  --signing-key /path/to/signing.key
```
