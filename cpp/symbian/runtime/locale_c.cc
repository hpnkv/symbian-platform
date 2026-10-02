// C-locale service missing from the selected OpenC ctype exports.
extern "C" int isblank(int value) {
  return value == ' ' || value == '\t';
}
