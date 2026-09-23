// Central include for Win32 + GDI+ with the short `gdi` alias used everywhere.
#ifndef PET_GDI_H
#define PET_GDI_H

#include <windows.h>
#include <wtypes.h>  // PROPID, required by gdiplus.h
#include <gdiplus.h>

namespace gdi = Gdiplus;

#endif  // PET_GDI_H
