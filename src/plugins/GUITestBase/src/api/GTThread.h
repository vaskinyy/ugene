/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2015 UniPro <ugene@unipro.ru>
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

#ifndef _U2_GT_THREAD_H_
#define _U2_GT_THREAD_H_

#include <QTimer>

#include <U2Test/CustomScenario.h>

namespace U2 {

class U2OpStatus;

class ThreadWaiter : public QObject {
    Q_OBJECT
public:
    ThreadWaiter(U2OpStatus &os);

    void wait();

private slots:
    void sl_timeout();

private:
    QTimer timer;
    U2OpStatus &os;
    qint64 startValue;
    qint64 endValue;
};

class GTThread {
public:
    static void waitForMainThread(U2OpStatus &os);
    static void runInMainThread(U2OpStatus &os, CustomScenario *scenario);
};

}   // namespace U2

#endif // _U2_GT_THREAD_H_