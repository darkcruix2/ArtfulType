#include <MacTypes.h>
#include <TextEdit.h>
void test(TEHandle te) {
  Rect savedView = (**te).viewRect;
  (**te).viewRect.top = (**te).viewRect.bottom;
}
