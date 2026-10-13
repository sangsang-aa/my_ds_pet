#include "bounds.h"

namespace {
SubjectBounds g_bounds;
}

void SetSubjectBounds(const SubjectBounds& bounds) {
    g_bounds = bounds;
}

SubjectBounds GetSubjectBounds() {
    return g_bounds;
}
