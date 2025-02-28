/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2013-2016 Symless Ltd.
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "common/common.h"

#import <CoreFoundation/CoreFoundation.h>

namespace inputleap {

#if defined(__cplusplus)
extern "C" {
#endif
void runCocoaApp();
void stopCocoaLoop();
void fakeDragging(const char* str, int cursorX, int cursorY);
CFStringRef getCocoaDropTarget();

#if defined(__cplusplus)
}
#endif

} // namespace inputleap
