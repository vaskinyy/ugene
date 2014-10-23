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

#include <U2Core/AppContext.h>
#include <U2Core/AppSettings.h>
#include <U2Core/L10n.h>
#include <U2Core/U2DbiRegistry.h>
#include <U2Core/U2DbiUtils.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/UdrDbi.h>
#include <U2Core/UdrRecord.h>
#include <U2Core/UdrSchemaRegistry.h>
#include <U2Core/UserApplicationsSettings.h>

#include "PrimerLibrary.h"

namespace U2 {

PrimerLibrary * PrimerLibrary::instance = NULL;
QMutex PrimerLibrary::mutex;
bool PrimerLibrary::released = false;

namespace {
    const QString libraryName = "primer_library.ugenedb";
    const UdrSchemaId PRIMER_UDR_ID = "Primer";
    const int NAME_FILED = 0;
    const int SEQ_FILED = 1;
    const int GC_FILED = 2;
    const int TM_FILED = 3;
}

PrimerLibrary * PrimerLibrary::getInstance(U2OpStatus &os) {
    QMutexLocker lock(&mutex);
    if (NULL != instance) {
        return instance;
    }
    if (released) {
        os.setError(L10N::nullPointerError("Primer Library"));
        return NULL;
    }

    released = true;
    { // create instance
        initPrimerUdr(os);
        CHECK_OP(os, NULL);

        UserAppsSettings *settings = AppContext::getAppSettings()->getUserAppsSettings();
        SAFE_POINT_EXT(NULL != settings, os.setError(L10N::nullPointerError("UserAppsSettings")), NULL);

        // open DBI connection
        const QString path = settings->getFileStorageDir() + "/" + libraryName;
        U2DbiRef dbiRef(DEFAULT_DBI_ID, path.toLocal8Bit());
        QHash<QString, QString> properties;
        properties[U2DbiOptions::U2_DBI_LOCKING_MODE] = "normal";
        QScopedPointer<DbiConnection> connection(new DbiConnection(dbiRef, true, os, properties)); // create if not exists
        CHECK_OP(os, NULL);

        instance = new PrimerLibrary(connection.take());
    }
    released = false;

    return instance;
}

void PrimerLibrary::release() {
    QMutexLocker lock(&mutex);
    delete instance;
    instance = NULL;
    released = true;
}

PrimerLibrary::PrimerLibrary(DbiConnection *connection)
: connection(connection), udrDbi(NULL)
{
    udrDbi = connection->dbi->getUdrDbi();
}

PrimerLibrary::~PrimerLibrary() {
    delete connection;
}

void PrimerLibrary::initPrimerUdr(U2OpStatus &os) {
    UdrSchema::FieldDesc name("name", UdrSchema::STRING);
    UdrSchema::FieldDesc sequence("sequence", UdrSchema::STRING);
    UdrSchema::FieldDesc gc("GC", UdrSchema::DOUBLE);
    UdrSchema::FieldDesc tm("Tm", UdrSchema::DOUBLE);

    QScopedPointer<UdrSchema> primerSchema(new UdrSchema(PRIMER_UDR_ID));
    primerSchema->addField(name, os);
    primerSchema->addField(sequence, os);
    primerSchema->addField(gc, os);
    primerSchema->addField(tm, os);
    CHECK_OP(os, );

    AppContext::getUdrSchemaRegistry()->registerSchema(primerSchema.data(), os);
    if (!os.hasError()) {
        primerSchema.take();
    }
}

void PrimerLibrary::addPrimer(Primer &primer, U2OpStatus &os) {
    QList<UdrValue> values;
    values << UdrValue(primer.name);
    values << UdrValue(primer.sequence);
    values << UdrValue(primer.gc);
    values << UdrValue(primer.tm);
    UdrRecordId record = udrDbi->addRecord(PRIMER_UDR_ID, values, os);
    CHECK_OP(os, );

    primer.id = record.getRecordId();
}

QList<Primer> PrimerLibrary::getPrimers(U2OpStatus &os) const {
    QList<Primer> result;

    QList<UdrRecord> records = udrDbi->getRecords(PRIMER_UDR_ID, os);
    CHECK_OP(os, result);

    foreach (const UdrRecord &record, records) {
        Primer primer;
        primer.id = record.getId().getRecordId();
        primer.name = record.getString(NAME_FILED, os);
        primer.sequence = record.getString(SEQ_FILED, os);
        primer.gc = record.getDouble(GC_FILED, os);
        primer.tm = record.getDouble(TM_FILED, os);
        CHECK_OP(os, result);
        result << primer;
    }

    return result;
}

void PrimerLibrary::removePrimer(const Primer &primer, U2OpStatus &os) {
    UdrRecordId recordId(PRIMER_UDR_ID, primer.id);
    udrDbi->removeRecord(recordId, os);
}

} // U2
