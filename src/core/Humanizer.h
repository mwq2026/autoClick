#pragma once

#include <cstdint>

namespace human {

void MoveTo(int x, int y, double speed);
void Click(int button);
void Scroll(int delta);

} // namespace human
