// 黑簿 — i18n 完整性：每个 StrId 的中英文都非空。
#include "i18n/strings.h"

import std;

int main() {
    int failures = 0;
    const int count = static_cast<int>(heibu::StrId::Count);
    for (int i = 0; i < count; ++i) {
        const auto id = static_cast<heibu::StrId>(i);
        const std::string_view zh = heibu::tr(id, heibu::Locale::Zh);
        const std::string_view en = heibu::tr(id, heibu::Locale::En);
        if (zh.empty()) {
            std::println("FAIL: StrId {} zh empty", i);
            ++failures;
        }
        if (en.empty()) {
            std::println("FAIL: StrId {} en empty", i);
            ++failures;
        }
    }
    if (failures == 0) {
        std::println("test_i18n: ok ({} entries, zh+en complete)", count);
        return 0;
    }
    std::println("test_i18n: {} failure(s)", failures);
    return 1;
}
