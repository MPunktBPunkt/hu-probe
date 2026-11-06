/**
 * Host-only B1 regression: usb_gadget mode validation + sticky load precedence.
 *
 *   make -C tools/gadget_nvs test
 */
#include "core/GadgetMode.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
            ++g_fail;                                                                          \
        }                                                                                      \
    } while (0)

static void test_valid_modes() {
    CHECK(huprobe::gadgetModeValid("msc"));
    CHECK(huprobe::gadgetModeValid("aoa_observe"));
    CHECK(huprobe::gadgetModeValid("aoa_phone_like"));
    CHECK(huprobe::gadgetModeValid("aoa_id_303a"));
    CHECK(!huprobe::gadgetModeValid(""));
    CHECK(!huprobe::gadgetModeValid(nullptr));
    CHECK(!huprobe::gadgetModeValid("aoa_observe "));
    CHECK(!huprobe::gadgetModeValid("composite"));
    CHECK(!huprobe::gadgetModeValid("AOA_OBSERVE"));
}

static void test_vendor_only_isolation() {
    CHECK(huprobe::gadgetModeIsAoaVendorOnly("aoa_observe"));
    CHECK(huprobe::gadgetModeIsAoaVendorOnly("aoa_phone_like"));
    CHECK(huprobe::gadgetModeIsAoaVendorOnly("aoa_id_303a"));
    CHECK(!huprobe::gadgetModeIsAoaVendorOnly("msc"));
    CHECK(!huprobe::gadgetModeIsAoaVendorOnly(nullptr));
    CHECK(huprobe::gadgetModeIsMsc("msc"));
    CHECK(!huprobe::gadgetModeIsMsc("aoa_observe"));
    CHECK(huprobe::gadgetModeIsPhoneLike("aoa_phone_like"));
    CHECK(huprobe::gadgetModeIsId303a("aoa_id_303a"));
    CHECK(!huprobe::gadgetModeIsId303a("aoa_observe"));
}

static void test_sticky_precedence() {
    // B1 fix: hpgad sticky wins over crowded huprobe legacy key.
    CHECK(std::strcmp(huprobe::resolveGadgetMode("msc", "aoa_observe"), "msc") == 0);
    CHECK(std::strcmp(huprobe::resolveGadgetMode("aoa_phone_like", "aoa_observe"),
                      "aoa_phone_like") == 0);
    CHECK(std::strcmp(huprobe::resolveGadgetMode("aoa_id_303a", "msc"), "aoa_id_303a") == 0);
    // Empty/invalid sticky → fall back to legacy.
    CHECK(std::strcmp(huprobe::resolveGadgetMode("", "aoa_observe"), "aoa_observe") == 0);
    CHECK(std::strcmp(huprobe::resolveGadgetMode(nullptr, "aoa_phone_like"), "aoa_phone_like") ==
          0);
    CHECK(std::strcmp(huprobe::resolveGadgetMode("bogus", "msc"), "msc") == 0);
    // Both invalid → default msc (never silently stick to observe).
    CHECK(std::strcmp(huprobe::resolveGadgetMode(nullptr, nullptr), "msc") == 0);
    CHECK(std::strcmp(huprobe::resolveGadgetMode("", "nope"), "msc") == 0);
}

static void test_soft_detach_invariant() {
    CHECK(huprobe::softDetachChangesGadgetMode() == false);
}

int main() {
    test_valid_modes();
    test_vendor_only_isolation();
    test_sticky_precedence();
    test_soft_detach_invariant();
    if (g_fail) {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::puts("gadget_nvs: OK");
    return 0;
}
