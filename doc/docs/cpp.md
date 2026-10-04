# Native C++ reference

The reference is easiest to use after choosing the layer your code touches.
The SDK owns application services and build tools; original Symbian headers
describe OS interfaces that an application may call through verified imports.
The [SDK native API guide](reference/native-sdk.md) explains targets, results
and lifetime before the generated symbol detail.

| What you are doing | Read first | Symbol detail |
| --- | --- | --- |
| Writing an application | [C++ usage](capabilities/cpp.md) and [runtime support](capabilities/runtime.md) | [SDK C++ API](cpp/index.html) |
| Using display, storage, power or camera services | [Device API map](capabilities/device-apis.md) | [SDK C++ API](cpp/index.html) |
| Inspecting E32 or SIS artifacts on the host | [SDK exports](reference/sdk.md) | [SDK C++ API](cpp/index.html) |
| Calling EUSER, Window Server or file APIs directly | [Native Symbian interfaces](reference/native-symbian.md) | [Original header reference](cpp/platform/index.html) |

## How to read the generated pages

The [SDK C++ reference](cpp/index.html) has a task-oriented landing page,
namespace index, file list and searchable declarations. Start with a public
header: `symbian/api/...` for guest services, `symbian/concurrency/...` for
asynchronous work, or `symbian/e32` and `symbian/sis` for host format tools.
Implementation files appear to help trace behavior; they are not additional
application APIs.

The [original header reference](cpp/platform/index.html) is deliberately
separate and covers nine selected historical headers. Its declarations are a
research aid. Check the SDK import proxy and named firmware evidence before
depending on an OS symbol. Generated output by itself does not establish
physical Nokia 808 compatibility.
