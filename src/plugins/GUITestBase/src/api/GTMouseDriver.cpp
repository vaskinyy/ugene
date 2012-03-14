/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2012 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifdef _WIN32
#include <windows.h>
#endif

#include "GTMouseDriver.h"
#include "QtUtils.h"

namespace U2 {

#ifdef _WIN32

void GTMouseDriver::moveTo(U2::U2OpStatus &os, const QPoint& p)
{
    // get screen resolution
    HDC hDCScreen = GetDC(NULL);
    int horres = GetDeviceCaps(hDCScreen, HORZRES);
    int vertres = GetDeviceCaps(hDCScreen, VERTRES);
    ReleaseDC(NULL, hDCScreen);

    QRect screen(0, 0, horres-1, vertres-1);
    CHECK_SET_ERR(screen.contains(p), "Invalid coordinates for moveTo()");

    const int points_in_line = 65535;
    const double points_in_x_pixel = points_in_line / static_cast<double>(horres);
    const double points_in_y_pixel = points_in_line / static_cast<double>(vertres);

    POINT pos;
    GetCursorPos(&pos);
	
    int x0 = pos.x;
    int y0 = pos.y;
    int x1 = p.x();
    int y1 = p.y();
	
    INPUT event;
    event.type = INPUT_MOUSE;
    event.mi.dx = 0;
    event.mi.dy = 0;
    event.mi.mouseData = 0;
    event.mi.dwFlags =  MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    event.mi.time = 0;
    event.mi.dwExtraInfo = 0;

    if (x0 == x1) {
        event.mi.dx = x0 * points_in_x_pixel + 0.5;
        while(y0 != y1) {
            if (y0 < y1) {
                ++y0;
            } else {
                --y0;
            }
            event.mi.dy = y0 * points_in_y_pixel + 0.5;
            SendInput(1, &event, sizeof(event));
            Sleep(5);
        }
    } else if (y0 == y1) {
        event.mi.dy = y0 * points_in_y_pixel + 0.5;
        while(x0 != x1) {
            if (x0 < x1) {
                ++x0;
            } else {
                --x0;
            }
            event.mi.dx = x0 * points_in_x_pixel + 0.5;
            SendInput(1, &event, sizeof(event));
            Sleep(5);
        }
    } else {
        // moved by the shortest way
        // equation of the line by two points y = (-(x0 * y1 - x1 * y0) - x*(y0 - y1)) / (x1 - x0) 
        int diff_x = x1 - x0;
        int diff_y = y0 - y1;
        int diff_xy = -(x0 * y1 - x1 * y0);
        int current_x = x0, current_y;

        while (current_x != x1) {
            if (x1 > x0) {
                ++current_x;
            } else {
                -- current_x;
            }

            current_y = (diff_xy - current_x * diff_y) / diff_x;
            event.mi.dy = current_y * points_in_y_pixel + 0.5;
            event.mi.dx = current_x * points_in_x_pixel + 0.5;
            SendInput(1, &event, sizeof(event));
		
            Sleep(5);
        }
    }
}

void GTMouseDriver::press(U2::U2OpStatus &os, ButtonType button_type)
{   
    unsigned int buttons[3] = {MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_MIDDLEDOWN};

    INPUT event;
    event.type = INPUT_MOUSE;
    event.mi.dx = 0;
    event.mi.dy = 0;
    event.mi.mouseData = 0;
    event.mi.dwFlags = buttons[button_type];
    event.mi.time = 0;
    event.mi.dwExtraInfo = 0;

    SendInput(1, &event, sizeof(event));
}

void GTMouseDriver::release(U2::U2OpStatus &os, ButtonType button_type)
{
    // TODO: check if this key has been already pressed
    unsigned int buttons[3] = {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP};

    INPUT event;
    event.type = INPUT_MOUSE;
    event.mi.dx = 0;
    event.mi.dy = 0;
    event.mi.mouseData = 0;
    event.mi.dwFlags = buttons[button_type];
    event.mi.time = 0;
    event.mi.dwExtraInfo = 0;

    SendInput(1, &event, sizeof(event));
}

void GTMouseDriver::click(U2::U2OpStatus &os, ButtonType button_type)
{
    press(os, button_type);
    Sleep(10);
    release(os, button_type);
}

void GTMouseDriver::doubleClick(U2OpStatus &os)
{
    click(os, LEFT);
    Sleep(10);
    click(os, LEFT);
}

void GTMouseDriver::scroll(U2OpStatus &os, int value)
{
    INPUT event;
    event.type = INPUT_MOUSE;
    event.mi.dx = 0;
    event.mi.dy = 0;
    event.mi.mouseData = value * WHEEL_DELTA;
    event.mi.dwFlags = MOUSEEVENTF_WHEEL;
    event.mi.time = 0;
    event.mi.dwExtraInfo = 0;

    SendInput(1, &event, sizeof(event));
}

#endif // _WIN32
} //namespace

