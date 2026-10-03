#pragma once
#include "documents/Document.h"
#include <QString>
namespace scalar {
// Only delimited math is passed to the local runtime. Ordinary text never needs it.
std::vector<MathFragment> mathFragments(const std::string& source);
QString renderMath(std::vector<MathFragment>& fragments);
QString mathRuntimeDirectory();
}
