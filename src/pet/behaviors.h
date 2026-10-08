// The default behavior registry: every behavior the pet can perform, described
// as data. Adding an entry here plus a matching folder under
// desktop_pet_image/<action>/ makes the pet able to do it — the core loop in
// pet.cpp needs no change.
#ifndef PET_BEHAVIORS_H
#define PET_BEHAVIORS_H

#include <vector>

#include "behavior.h"

const std::vector<pet_behavior::Spec>& DefaultBehaviors();

#endif  // PET_BEHAVIORS_H
