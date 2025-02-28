/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
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

#include "inputleap/KeyState.h"

#include <Carbon/Carbon.h>

#include <map>
#include <set>
#include <vector>

namespace inputleap {

typedef TISInputSourceRef KeyLayout;
class IOSXKeyResource;

//! OS X key state
/*!
A key state for OS X.
*/
class OSXKeyState : public KeyState {
public:
    typedef std::vector<KeyID> KeyIDs;

    OSXKeyState(IEventQueue* events);
    OSXKeyState(IEventQueue* events, inputleap::KeyMap& keyMap);
    virtual ~OSXKeyState();

    //! @name modifiers
    //@{

    //! Handle modifier key change
    /*!
    Determines which modifier keys have changed and updates the modifier
    state and sends key events as appropriate.
    */
    void handleModifierKeys(void* target,
                            KeyModifierMask oldMask, KeyModifierMask newMask);

    //@}
    //! @name accessors
    //@{

    //! Convert OS X modifier mask to InputLeap mask
    /*!
    Returns the InputLeap modifier mask corresponding to the OS X modifier
    mask in \p mask.
    */
    KeyModifierMask mapModifiersFromOSX(std::uint32_t mask) const;

    //! Convert CG flags-style modifier mask to old-style Carbon
    /*!
    Still required in a few places for translation calls.
    */
    KeyModifierMask mapModifiersToCarbon(std::uint32_t mask) const;

    //! Map key event to keys
    /*!
    Converts a key event into a sequence of KeyIDs and the shadow modifier
    state to a modifier mask.  The KeyIDs list, in order, the characters
    generated by the key press/release.  It returns the id of the button
    that was pressed or released, or 0 if the button doesn't map to a known
    KeyID.
    */
    KeyButton mapKeyFromEvent(KeyIDs& ids,
                            KeyModifierMask* maskOut, CGEventRef event) const;

    //! Map key and mask to native values
    /*!
    Calculates mac virtual key and mask for a key \p key and modifiers
    \p mask.  Returns \c true if the key can be mapped, \c false otherwise.
    */
    bool map_hot_key_to_mac(KeyID key, KeyModifierMask mask, std::uint32_t& macVirtualKey,
                            std::uint32_t& macModifierMask) const;

    //@}

    // IKeyState overrides
    virtual bool fakeCtrlAltDel();
    virtual bool fakeMediaKey(KeyID id);
    virtual KeyModifierMask pollActiveModifiers() const;
    virtual std::int32_t pollActiveGroup() const;
    virtual void pollPressedKeys(KeyButtonSet& pressedKeys) const;

    CGEventFlags getModifierStateAsOSXFlags();
protected:
    // KeyState overrides
    virtual void getKeyMap(inputleap::KeyMap& keyMap);
    virtual void fakeKey(const Keystroke& keystroke);

private:
    class KeyResource;
    typedef std::vector<KeyLayout> GroupList;

    // Add hard coded special keys to a inputleap::KeyMap.
    void getKeyMapForSpecialKeys(inputleap::KeyMap& keyMap, std::int32_t group) const;

    // Convert keyboard resource to a key map
    bool getKeyMap(inputleap::KeyMap& keyMap, std::int32_t group, const IOSXKeyResource& r) const;

    // Get the available keyboard groups
    bool getGroups(GroupList&) const;

    // Change active keyboard group to group
    void setGroup(std::int32_t group);

    // Check if the keyboard layout has changed and update keyboard state
    // if so.
    void checkKeyboardLayout();

    // Send an event for the given modifier key
    void handleModifierKey(void* target, std::uint32_t virtualKey, KeyID id, bool down,
                           KeyModifierMask newMask);

    // Checks if any in \p ids is a glyph key and if \p isCommand is false.
    // If so it adds the AltGr modifier to \p mask.  This allows OS X
    // servers to use the option key both as AltGr and as a modifier.  If
    // option is acting as AltGr (i.e. it generates a glyph and there are
    // no command modifiers active) then we don't send the super modifier
    // to clients because they'd try to match it as a command modifier.
    void adjustAltGrModifier(const KeyIDs& ids,
                            KeyModifierMask* mask, bool isCommand) const;

    // Maps an OS X virtual key id to a KeyButton.  This simply remaps
    // the ids so we don't use KeyButton 0.
    static KeyButton mapVirtualKeyToKeyButton(std::uint32_t keyCode);

    // Maps a KeyButton to an OS X key code.  This is the inverse of
    // mapVirtualKeyToKeyButton.
    static std::uint32_t mapKeyButtonToVirtualKey(KeyButton keyButton);

    void init();

    // Post a key event to HID manager. It posts an event to HID client, a
    // much lower level than window manager which's the target from carbon
    // CGEventPost
    void postHIDVirtualKey(const std::uint8_t virtualKeyCode, const bool postDown);

private:
    // OS X uses a physical key if 0 for the 'A' key.  InputLeap reserves
    // KeyButton 0 so we offset all OS X physical key ids by this much
    // when used as a KeyButton and by minus this much to map a KeyButton
    // to a physical button.
    enum {
        KeyButtonOffset = 1
    };

    typedef std::map<CFDataRef, std::int32_t> GroupMap;
    typedef std::map<std::uint32_t, KeyID> VirtualKeyMap;

    VirtualKeyMap m_virtualKeyMap;
    mutable std::uint32_t m_deadKeyState;
    GroupList m_groups;
    GroupMap m_groupMap;
    bool m_shiftPressed;
    bool m_controlPressed;
    bool m_altPressed;
    bool m_superPressed;
    bool m_capsPressed;
};

} // namespace inputleap
