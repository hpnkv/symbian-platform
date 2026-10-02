// Host entry point for a CLion CMake Application run configuration.
#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

#include <unistd.h>

int main(int argc, char* argv[]) {
  const char* fixed[] = {SYMBIAN_LAUNCH_PYTHON, "-m", "symbian.emulator.launch",
                         "--root", SYMBIAN_LAUNCH_ROOT};
  std::vector<char*> arguments;
  arguments.reserve(6 + static_cast<size_t>(argc));
  for (const char* argument : fixed) {
    arguments.push_back(const_cast<char*>(argument));
  }
  for (int i = 1; i < argc; ++i) {
    arguments.push_back(argv[i]);
  }
  arguments.push_back(nullptr);
  // Replacing this process preserves Stop/SIGINT and the supervisor's owned
  // child lifetime. There is no detached emulator or second scheduler.
  execv(SYMBIAN_LAUNCH_PYTHON, arguments.data());
  const int error = errno;
  std::cerr << "Cannot start GUI supervisor using " << SYMBIAN_LAUNCH_PYTHON
            << ": " << std::strerror(error) << '\n';
  return 127;
}
