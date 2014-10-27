/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2014 UniPro <ugene@unipro.ru>
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

#ifndef _U2_PRIMERLIBRARY_H_
#define _U2_PRIMERLIBRARY_H_

#include <QMutex>

#include <U2Core/U2OpStatus.h>
#include <U2Core/UdrSchema.h>

#include "Primer.h"

namespace U2 {

class DbiConnection;
class UdrDbi;

class PrimerLibrary {
    Q_DISABLE_COPY(PrimerLibrary)
public:
    ~PrimerLibrary();

    static PrimerLibrary * getInstance(U2OpStatus &os);
    static void release();

    void addPrimer(Primer &primer, U2OpStatus &os);
    QList<Primer> getPrimers(U2OpStatus &os) const;
    void removePrimer(const Primer &primer, U2OpStatus &os);

private:
    static void initPrimerUdr(U2OpStatus &os);

    PrimerLibrary(DbiConnection *connection);

private:
    static QScopedPointer<PrimerLibrary> instance;
    static QMutex mutex;

    DbiConnection *connection;
    UdrDbi *udrDbi;
};

} // U2

#endif // _U2_PRIMERLIBRARY_H_