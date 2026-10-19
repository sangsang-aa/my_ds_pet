// Topmost 调整大小 dialog: a trackbar slider (30%..150%) with live readout.
#ifndef PET_SCALE_DIALOG_H
#define PET_SCALE_DIALOG_H

#include <windows.h>

#include <functional>

namespace tray {

// Opens (or re-creates) the modal-look resize dialog. `onApply` is invoked with
// the selected percent when 确定 is pressed; 取消 / ESC / close discard it.
void ShowScaleDialog(HWND owner, HINSTANCE instance, int currentPercent,
                     std::function<void(int)> onApply);

}  // namespace tray

#endif  // PET_SCALE_DIALOG_H
