This bounded native-leave probe checks repeated nested original `User::Leave`
and `TRAPD`, C++ destructor execution, oversized native allocation failure
and a balanced heap-cell count. `SYMBIAN_LEAVE_EXPECTED=33` supplies a negative
control (normal count is 32). It requires the SDK's explicit native leave source
boundary; general hosted C++ exception support is a separate capability.
