// The subject's rectangle in screen coordinates. The pet publishes it every
// frame so the speech bubble and the head-sweep band follow the character
// instead of the (larger) canvas frame it sits in.
#ifndef PET_BOUNDS_H
#define PET_BOUNDS_H

struct SubjectBounds {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool valid = false;
};

void SetSubjectBounds(const SubjectBounds& bounds);
SubjectBounds GetSubjectBounds();

#endif  // PET_BOUNDS_H
