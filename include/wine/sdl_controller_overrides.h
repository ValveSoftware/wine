/*
 * Copyright 2021 Arkadiusz Hiler for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __SDL_CONTROLLER_OVERRIDES_H
#define __SDL_CONTROLLER_OVERRIDES_H

#define VID_SONY 0x054c
#define PID_SONY_DUALSHOCK_4 0x05c4
#define PID_SONY_DUALSHOCK_4_2 0x09cc
#define PID_SONY_DUALSHOCK_4_DONGLE 0x0ba0
#define PID_SONY_DUALSENSE 0x0ce6

#define VID_VALVE 0x28de
#define PID_VALVE_VIRTUAL_CONTROLLER 0x11ff

#define VID_MICROSOFT 0x045e
#define PID_MICROSOFT_XBOX_360 0x028e
#define PID_MICROSOFT_XBOX_360_WIRELESS 0x028f
#define PID_MICROSOFT_XBOX_360_ADAPTER  0x0719
#define PID_MICROSOFT_XBOX_ONE 0x02d1
#define PID_MICROSOFT_XBOX_ONE_CF 0x02dd
#define PID_MICROSOFT_XBOX_ONE_ELITE 0x02e3
#define PID_MICROSOFT_XBOX_ONE_S 0x02ea
#define PID_MICROSOFT_XBOX_ONE_S_WIRELESS 0x02fd
#define PID_MICROSOFT_XBOX_ELITE_2 0x0B00
#define PID_MICROSOFT_XBOX_ELITE_2_WIRELESS 0x0B05
#define PID_MICROSOFT_XBOX_SERIES 0x0b12
#define PID_MICROSOFT_XBOX_SERIES_WIRELESS 0x0b13

static BOOL __controller_hack_sdl_is_vid_pid_xbox_360(WORD vid, WORD pid)
{
    if (vid != VID_MICROSOFT)
        return FALSE;

    if (pid == PID_MICROSOFT_XBOX_360 ||
        pid == PID_MICROSOFT_XBOX_360_WIRELESS ||
        pid == PID_MICROSOFT_XBOX_360_ADAPTER)
        return TRUE;

    return FALSE;
}
static BOOL __controller_hack_sdl_is_vid_pid_xbox_one(WORD vid, WORD pid) {
    if (vid != VID_MICROSOFT)
        return FALSE;

    if (pid == PID_MICROSOFT_XBOX_ONE ||
        pid == PID_MICROSOFT_XBOX_ONE_CF ||
        pid == PID_MICROSOFT_XBOX_ONE_ELITE ||
        pid == PID_MICROSOFT_XBOX_ONE_S ||
        pid == PID_MICROSOFT_XBOX_ONE_S_WIRELESS ||
        pid == PID_MICROSOFT_XBOX_ELITE_2 ||
        pid == PID_MICROSOFT_XBOX_ELITE_2_WIRELESS ||
        pid == PID_MICROSOFT_XBOX_SERIES ||
        pid == PID_MICROSOFT_XBOX_SERIES_WIRELESS)
        return TRUE;

    return FALSE;
}

static BOOL __controller_hack_sdl_is_vid_pid_sony_dualshock4(WORD vid, WORD pid)
{
    if (vid != VID_SONY)
        return FALSE;

    if (pid == PID_SONY_DUALSHOCK_4 ||
        pid == PID_SONY_DUALSHOCK_4_2 ||
        pid == PID_SONY_DUALSHOCK_4_DONGLE)
        return TRUE;

    return FALSE;
}

static BOOL __controller_hack_sdl_is_vid_pid_sony_dualsense(WORD vid, WORD pid)
{
    if (vid == VID_SONY && pid == PID_SONY_DUALSENSE)
        return TRUE;

    return FALSE;
}

static void __controller_hack_sdl_vid_pid_override(WORD *vid, WORD *pid, const char *sdl_joystick_name)
{
    TRACE("(*vid = %04hx, *pid = *%04hx\n", *vid, *pid);
    if (*vid == VID_VALVE && *pid == PID_VALVE_VIRTUAL_CONTROLLER)
    {
        TRACE("Valve Virtual Controller found - pretending it's x360\n");
        *vid = VID_MICROSOFT;
        *pid = PID_MICROSOFT_XBOX_360;
    }
    else if (sdl_joystick_name &&
             (strstr(sdl_joystick_name, "Xbox") ||
              strstr(sdl_joystick_name, "XBOX") ||
              strstr(sdl_joystick_name, "X-Box")) &&
             !(__controller_hack_sdl_is_vid_pid_xbox_one(*vid, *pid) ||
               __controller_hack_sdl_is_vid_pid_xbox_360(*vid, *pid)))
    {
        WARN("Unknown xinput controller found - pretending it's x360\n");

        *vid = VID_MICROSOFT;
        *pid = PID_MICROSOFT_XBOX_360;
    }
}

static void __controller_hack_sdl_name_override(WORD vid, WORD pid, const char **name)
{
    const char *new_name = NULL;

    TRACE("vid = %04hx, pid = %04hx, *name = %s\n", vid, pid, debugstr_a(*name));

    if (__controller_hack_sdl_is_vid_pid_xbox_360(vid, pid))
        new_name = "Controller (XBOX 360 For Windows)";
    else if (__controller_hack_sdl_is_vid_pid_xbox_one(vid, pid))
        new_name = "Controller (Xbox One For Windows)";
    else if (__controller_hack_sdl_is_vid_pid_sony_dualshock4(vid, pid) ||
             __controller_hack_sdl_is_vid_pid_sony_dualsense(vid, pid))
        new_name = "Wireless Controller";

    if (new_name)
    {
        TRACE("new name for the controller = %s\n", debugstr_a(new_name));
        *name = new_name;
    }
}
#endif /* __SDL_CONTROLLER_OVERRIDES_H */
