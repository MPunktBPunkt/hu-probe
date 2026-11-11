/**
 * Host-only field plan regression: validation, run -> job mapping, scenario names.
 *
 *   make -C tools/field_plan test
 */
#include "core/FieldPlanCore.h"

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

static huprobe::Plan makePlan() {
    huprobe::Plan p{};
    std::strcpy(p.id, "2026-11-12a");
    std::strcpy(p.vehicle, "bmw");
    p.enabled = true;
    p.jobCount = 2;
    std::strcpy(p.jobs[0].id, "mix");
    std::strcpy(p.jobs[0].experiment, "drive_home");
    std::strcpy(p.jobs[0].media, "drive_mix");
    p.jobs[0].repeat = 3;
    std::strcpy(p.jobs[1].id, "big8");
    std::strcpy(p.jobs[1].experiment, "drive_home");
    p.jobs[1].fine = true;
    p.jobs[1].measureS = 1200;
    p.jobs[1].repeat = 2;
    return p;
}

static void test_tokens() {
    CHECK(huprobe::planTokenValid("abc_1-X", 13));
    CHECK(!huprobe::planTokenValid("", 13));
    CHECK(!huprobe::planTokenValid(nullptr, 13));
    CHECK(!huprobe::planTokenValid("a/b", 13));
    CHECK(!huprobe::planTokenValid("a b", 13));
    CHECK(huprobe::planTokenValid("123456789012", 13));
    CHECK(!huprobe::planTokenValid("1234567890123", 13));
}

static void test_validate() {
    huprobe::Plan p = makePlan();
    CHECK(huprobe::planValidate(p) == nullptr);

    huprobe::Plan q = p;
    std::strcpy(q.jobs[1].id, "mix");
    CHECK(std::strcmp(huprobe::planValidate(q), "dup_job_id") == 0);

    q = p;
    q.jobs[0].repeat = 0;
    CHECK(std::strcmp(huprobe::planValidate(q), "bad_repeat") == 0);

    q = p;
    q.jobs[1].measureS = 1801;
    CHECK(std::strcmp(huprobe::planValidate(q), "bad_measure") == 0);

    q = p;
    q.jobCount = 0;
    CHECK(std::strcmp(huprobe::planValidate(q), "no_jobs") == 0);

    q = p;
    q.vehicle[0] = '\0';
    CHECK(std::strcmp(huprobe::planValidate(q), "bad_vehicle") == 0);

    q = p;
    std::strcpy(q.jobs[0].media, "x/y");
    CHECK(std::strcmp(huprobe::planValidate(q), "bad_media_name") == 0);

    q = p;
    std::strcpy(q.jobs[1].stall, R"({"regions":["body"],"pace":"realtime","count":0})");
    CHECK(huprobe::planValidate(q) == nullptr);
    CHECK(sizeof(q.jobs[1].stall) == huprobe::kPlanStallLen);
}

static void test_runs() {
    const huprobe::Plan p = makePlan();
    CHECK(huprobe::planTotalRuns(p) == 5);
    uint8_t rep = 0;
    const huprobe::PlanJob* j = huprobe::planJobForRun(p, 0, &rep);
    CHECK(j == &p.jobs[0] && rep == 1);
    j = huprobe::planJobForRun(p, 2, &rep);
    CHECK(j == &p.jobs[0] && rep == 3);
    j = huprobe::planJobForRun(p, 3, &rep);
    CHECK(j == &p.jobs[1] && rep == 1);
    j = huprobe::planJobForRun(p, 4, &rep);
    CHECK(j == &p.jobs[1] && rep == 2);
    CHECK(huprobe::planJobForRun(p, 5, &rep) == nullptr);
}

static void test_scenario() {
    huprobe::Plan p = makePlan();
    char buf[32];
    CHECK(huprobe::planScenario(p, 3, buf, sizeof(buf)));
    CHECK(std::strcmp(buf, "fp_2026-11-12a_big8_r1") == 0);
    CHECK(!huprobe::planScenario(p, 5, buf, sizeof(buf)));
    char small[8];
    CHECK(!huprobe::planScenario(p, 0, small, sizeof(small)));

    std::strcpy(p.id, "123456789012");
    std::strcpy(p.jobs[0].id, "12345678");
    p.jobs[0].repeat = 50;
    CHECK(huprobe::planValidate(p) == nullptr);
    CHECK(huprobe::planScenario(p, 49, buf, sizeof(buf)));
    CHECK(std::strlen(buf) <= 28);
}

int main() {
    test_tokens();
    test_validate();
    test_runs();
    test_scenario();
    if (g_fail) {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::puts("field_plan: all checks passed");
    return 0;
}
