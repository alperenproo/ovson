#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace CrashDumps {

struct Entry {
    std::wstring path;        // full path on disk
    std::wstring fileName;    // just the leaf for display
    uint64_t     sizeBytes = 0;
    FILETIME     writeTime{}; // for sort + "X ago" formatting
};

std::vector<Entry> scan();
std::wstring dumpsDir();
int deleteAll();
void openInExplorer(const std::wstring &entry);

} // namespace CrashDumps
