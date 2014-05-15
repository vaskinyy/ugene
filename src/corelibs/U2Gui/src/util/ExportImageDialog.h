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

#ifndef _U2_EXPORT_IMAGE_DIALOG_H_
#define _U2_EXPORT_IMAGE_DIALOG_H_


#include "ImageExporter.h"
#include <U2Gui/LastUsedDirHelper.h>

#include <QtCore/QList>
#include <QtCore/QString>
#if (QT_VERSION < 0x050000) //Qt 5
#include <QtGui/QDialog>
#else
#include <QtWidgets/QDialog>
#endif


class Ui_ImageExportForm;
class QRadioButton;
class QAbstractButton;
class QButtonGroup;

namespace U2 {

class U2GUI_EXPORT ExportImageDialog : public QDialog {
    Q_OBJECT
public:
    ExportImageDialog(ImageExporter* exporter,
                      QWidget* parent = NULL,
                      const QString& file = QString("untitled"));
    ExportImageDialog(const QList<ImageExporter*> &exporters,
                      ImageExporter* defaultExporter,
                      QWidget* parent = NULL, const QString& file = QString("untitled"));
    ExportImageDialog(QWidget* screenShotWidget,
                      ImageExporter::ImageScalingPolicy scalingPolicy = ImageExporter::Constant,
                      ImageExporter::FormatPolicy formatPolicy = ImageExporter::IgnoreVectorFormats,
                      QWidget* parent = NULL, const QString& file = QString("untitled"));

    const QString& getFilename() const { return filename; }
    const QString& getFormat() const { return format; }
    int getWidth() const;
    int getHeight() const;

    bool hasQuality() const;
    int getQuality() const;

public slots:
    void accept();

private slots:
    void sl_onBrowseButtonClick();
    void sl_onFormatsBoxItemChanged(const QString& text);
    void sl_onExporterSelected(QAbstractButton*);

private:
    void setupComponents();
    void setupExporters();
    void setSizeControlsEnabled(bool enabled);
    void setVectorFormats();

    static bool isVectorGraphicFormat(const QString &formatName);
    static bool isLossyFormat(const QString &formatName);
    static int getVectorFormatIdByName(const QString &formatName);

private:
    QList <ImageExporter*>                  exporters;
    ImageExporter*                          currentExporter;
    QMap <QRadioButton*, ImageExporter*>    exportersButtons;
    QButtonGroup*    exportersGroup;

    QList<QString>      supportedFormats;
    QString filename;
    QString origFilename;
    QString format;

    LastUsedDirHelper lod;
    Ui_ImageExportForm* ui;
}; // class ExportImageDialog

} // namespace

#endif
