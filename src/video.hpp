#pragma once
#include <stdint.h>
#include "generated/display_config.hpp"
namespace video {
constexpr unsigned max_rows=144, rows=A8_DISPLAY_ROWS, stride=40;
enum class Method { ScaleNearest, Crop, Full144, Full192 };
struct Options {unsigned height=rows;Method method=Method::ScaleNearest;unsigned top=0;};
bool convert(const uint8_t *full,uint8_t *dest,const Options& options);
}
