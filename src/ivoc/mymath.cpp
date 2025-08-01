#ifndef __INTEL_LLVM_COMPILER
#ifdef __clang__
#pragma float_control(precise, on)
#endif
#pragma STDC FENV_ACCESS ON
#endif

#include <../../nrnconf.h>
#include <InterViews/geometry.h>
#include "mymath.h"
#include "code.h"
#include "classreg.h"
#include "oc2iv.h"
#include <cmath>
#include <cstdio>
#include <cfenv>

static double distance_to_line(void*) {
    return MyMath::distance_to_line(
        *getarg(1), *getarg(2), *getarg(3), *getarg(4), *getarg(5), *getarg(6));
}

static double distance_to_line_segment(void*) {
    return MyMath::distance_to_line_segment(
        *getarg(1), *getarg(2), *getarg(3), *getarg(4), *getarg(5), *getarg(6));
}

static double inside(void*) {
    hoc_return_type_code = HocReturnType::boolean;
    return MyMath::inside(*getarg(1), *getarg(2), *getarg(3), *getarg(4), *getarg(5), *getarg(6));
}

int nrn_feround(int);
// return last rounding mode and set to given mode if 1,2,3,4.
// order is FE_DOWNWARD, FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD
static int round_mode[] = {FE_DOWNWARD, FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD};
int nrn_feround(int mode) {
    int oldmode = std::fegetround();
    if (oldmode == FE_TONEAREST) {
        oldmode = 2;
    } else if (oldmode == FE_TOWARDZERO) {
        oldmode = 3;
    } else if (oldmode == FE_UPWARD) {
        oldmode = 4;
    } else if (oldmode == FE_DOWNWARD) {
        oldmode = 1;
    } else {
        assert(0);
    }
    if (mode > 0 && mode < 5) {
        nrn_assert(std::fesetround(round_mode[mode - 1]) == 0);
    }
    return oldmode;
}

static double feround(void*) {
    int arg = 0;
    hoc_return_type_code = HocReturnType::integer;
    if (ifarg(1)) {
        arg = (int) chkarg(1, 0, 4);
    }
    return (double) nrn_feround(arg);
}

static Member_func members[] = {{"d2line", distance_to_line},
                                {"d2line_seg", distance_to_line_segment},
                                {"inside", inside},
                                {"feround", feround},
                                {nullptr, nullptr}};

static void* cons(Object*) {
    return NULL;
}

static void destruct(void*) {}

void GUIMath_reg() {
    class2oc("GUIMath", cons, destruct, members, nullptr, nullptr);
}

double MyMath::anint(double x) {
    if (x < 0) {
        return ceil(x - .5);
    } else {
        return floor(x + .5);
    }
}
float MyMath::min(int count, const float* x) {
    float m = x[0];
    for (int i = 1; i < count; ++i) {
        if (m > x[i]) {
            m = x[i];
        }
    }
    return m;
}

float MyMath::max(int count, const float* x) {
    float m = x[0];
    for (int i = 1; i < count; ++i) {
        if (m < x[i]) {
            m = x[i];
        }
    }
    return m;
}

// within epsilon distance from the infinite line

bool MyMath::near_line(Coord x, Coord y, Coord x1, Coord y1, Coord x2, Coord y2, float epsilon) {
    // printf("near_line %g %g %g %g %g %g %g\n", x,y,x1,y1,x2,y2,epsilon);
    if (eq(x, x1, epsilon) && eq(y, y1, epsilon)) {
        return true;
    }
    if (eq(x1, x2, epsilon) && eq(y1, y2, epsilon)) {
        return false;
    }
    Coord d, norm, norm2, dot;
    Coord dx, dy, dx2, dy2;
    dx = x - x1;
    dy = y - y1;
    dx2 = x2 - x1;
    dy2 = y2 - y1;
    // printf("%g %g %g %g\n", dx, dy, dx2, dy2);
    norm2 = dx2 * dx2 + dy2 * dy2;
    norm = dx * dx + dy * dy;
    dot = dx * dx2 + dy * dy2;
    d = norm - dot * dot / norm2;
    // printf("near_line %g\n", d);
    return d <= epsilon * epsilon;
}

float MyMath::distance_to_line(Coord x, Coord y, Coord x1, Coord y1, Coord x2, Coord y2) {
    // printf("near_line %g %g %g %g %g %g %g\n", x,y,x1,y1,x2,y2,epsilon);
    Coord d, norm, norm2, dot;
    Coord dx, dy, dx2, dy2;
    dx = x - x1;
    dy = y - y1;
    dx2 = x2 - x1;
    dy2 = y2 - y1;
    // printf("%g %g %g %g\n", dx, dy, dx2, dy2);
    norm2 = dx2 * dx2 + dy2 * dy2;
    norm = dx * dx + dy * dy;
    dot = dx * dx2 + dy * dy2;
    if (norm2 == 0) {
        norm2 = 1.;
    }
    d = norm - dot * dot / norm2;
    if (d < 0.) {
        return 0.;
    }
    return sqrt(d);
}

float MyMath::distance_to_line_segment(Coord x, Coord y, Coord x1, Coord y1, Coord x2, Coord y2) {
    Coord norm, norm2, dot;
    Coord dx, dy, dx2, dy2;
    dx = x - x1;
    dy = y - y1;
    dx2 = x2 - x1;
    dy2 = y2 - y1;
    norm2 = dx2 * dx2 + dy2 * dy2;
    norm = dx * dx + dy * dy;

    if (norm2 == 0) {
        return sqrt(norm);
    }
    dot = dx * dx2 + dy * dy2;
    if (dot < 0) {
        return sqrt(norm);
    } else if (dot > norm2) {
        dx = x - x2;
        dy = y - y2;
        return sqrt(dx * dx + dy * dy);
    } else {
        dx = norm - dot * dot / norm2;
        if (dx <= 0) {
            return 0.;
        }
        return sqrt(dx);
    }
}

bool MyMath::near_line_segment(Coord x,
                               Coord y,
                               Coord x1,
                               Coord y1,
                               Coord x2,
                               Coord y2,
                               float epsilon) {
    Coord l = x1, b = y1, r = x2, t = y2;
    MyMath::minmax(l, r);
    MyMath::minmax(b, t);
    // printf("near_line_seg inside %d\n",inside(x, y, l-epsilon, b-epsilon, r+epsilon, t+epsilon));
    // printf("near_line_seg near_line %d\n", near_line(x, y, l, b, r, t, epsilon));
    return MyMath::inside(x, y, l - epsilon, b - epsilon, r + epsilon, t + epsilon) &&
           MyMath::near_line(x, y, x1, y1, x2, y2, epsilon);
}

void MyMath::round_range(Coord x1, Coord x2, double& y1, double& y2, int& ntic) {
    double d = x2 - x1;
    d = pow(10, floor(log10(d))) / 10;
    y1 = d * MyMath::anint(x1 / d);
    y2 = d * MyMath::anint(x2 / d);
    int i = int((y2 - y1) / d + .5);  // 10 < i < 100
                                      // printf("%d %g\n", i, dx);
    for (;;) {
        if (i % 3 == 0) {
            ntic = 3;
            return;
        } else if (i % 4 == 0) {
            ntic = 4;
            return;
        } else if (i % 5 == 0) {
            ntic = 5;
            return;
        }
        y1 -= d;
        y2 += d;
        i += 2;
    }
}

void MyMath::round_range_down(Coord x1, Coord x2, double& y1, double& y2, int& ntic) {
    double d = x2 - x1;
    double e = pow(10, floor(log10(d))) / 10;
    int i = int(d / e + .5);
    if (i > 20) {
        y1 = 5 * e * ceil(x1 / e / 5 - .01);
        y2 = 5 * e * floor(x2 / e / 5 + .01);
    } else {
        y1 = e * ceil(x1 / e - .01);
        y2 = e * floor(x2 / e + .01);
    }
    i = int((y2 - y1) / e + .5);
    // printf("%d %g\n", i, e);
    for (;;) {
        if (i % 3 == 0) {
            ntic = 3;
            return;
        } else if (i % 4 == 0) {
            ntic = 4;
            return;
        } else if (i % 5 == 0) {
            ntic = 5;
            return;
        }
        y1 -= e;
        ++i;
    }
}

double MyMath::round(float& x1, float& x2, int direction, int digits) {
    double d;
    if (x2 > x1) {
        d = x2 - x1;
    } else {
        d = std::abs(x1);
    }
    double e = pow(10, floor(log10(d)) + 1 - digits);
    switch (direction) {
    case Expand:
        x1 = e * floor(x1 / e);
        x2 = e * ceil(x2 / e);
        break;
    case Contract:
        x1 = e * ceil(x1 / e);
        x2 = e * floor(x2 / e);
        break;
    case Lower:
        x1 = e * floor(x1 / e);
        x2 = e * floor(x2 / e);
        break;
    case Higher:
        x1 = e * ceil(x1 / e);
        x2 = e * ceil(x2 / e);
        break;
    }
    return e;
}

void MyMath::box(Requisition& req, Coord& x1, Coord& y1, Coord& x2, Coord& y2) {
    Requirement& rx = req.x_requirement();
    x1 = -rx.alignment() * rx.natural();
    x2 = x1 + rx.natural();
    Requirement& ry = req.y_requirement();
    y1 = -ry.alignment() * ry.natural();
    y2 = y1 + ry.natural();
}

bool MyMath::unit_normal(Coord x, Coord y, Coord* perp) {
    float d = sqrt(x * x + y * y);
    if (d < 1e-6) {
        perp[0] = 0.;
        perp[1] = 1.;
        return false;
    }
    perp[0] = y / d;
    perp[1] = -x / d;
    return true;
}
