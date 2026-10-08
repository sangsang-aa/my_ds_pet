// The behavior registry, built once from the compiled-in defaults below and then
// patched by ApplySettingsText (pet.ini). Adding an entry here plus a matching
// folder under desktop_pet_image/<action>/ makes the pet able to perform it —
// the core loop in pet.cpp needs no change.
#ifndef PET_BEHAVIORS_H
#define PET_BEHAVIORS_H

#include <vector>

#include "behavior.h"

std::vector<pet_behavior::Spec>& MutableBehaviors();
const std::vector<pet_behavior::Spec>& Behaviors();

#endif  // PET_BEHAVIORS_H
