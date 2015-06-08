/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include <iostream>
#include <QSettings>
#include <QMessageBox>
#include <QTextCodec>
#include <QFontDialog>
#include "../external/libqxt/qxtspanslider.h"

#include "ftsound.h"
#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "gvwaveform.h"
#include "gvamplitudespectrum.h"
#include "gvphasespectrum.h"
#include "gvspectrogramwdialogsettings.h"

WDialogSettings::WDialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WDialogSettings)
{
    ui->setupUi(this);

    // Fill the comboboxes
    QList<QByteArray> avcodecs = QTextCodec::availableCodecs();
    qSort(avcodecs);
    for(int ai=0; ai<avcodecs.size(); ++ai)
        ui->cbLabelDefaultTextEncoding->addItem(avcodecs[ai]);
    ui->cbLabelDefaultTextEncoding->setCurrentText("UTF-8");

    setWindowIcon(QIcon(":/icons/settings.svg"));
    setWindowIconText("Settings");
    setWindowTitle("Settings");

    connect(ui->ckPlaybackAvoidClicksAddWindows, SIGNAL(toggled(bool)), this, SLOT(setCKAvoidClicksAddWindows(bool)));
    connect(ui->sbPlaybackButterworthOrder, SIGNAL(valueChanged(int)), this, SLOT(setSBButterworthOrderChangeValue(int)));
    connect(ui->sbPlaybackAvoidClicksWindowDuration, SIGNAL(valueChanged(double)), this, SLOT(setSBAvoidClicksWindowDuration(double)));
    connect(ui->btnSettingsSave, SIGNAL(clicked()), this, SLOT(settingsSave()));
    connect(ui->btnSettingsClear, SIGNAL(clicked()), this, SLOT(settingsClear()));  

    gMW->m_settings.add(ui->sbPlaybackButterworthOrder);
    gMW->m_settings.add(ui->cbPlaybackFilteringCompensateEnergy);
    gMW->m_settings.add(ui->ckPlaybackAvoidClicksAddWindows);
    gMW->m_settings.add(ui->sbPlaybackAvoidClicksWindowDuration);
    gMW->m_settings.add(ui->cbLabelDefaultTextEncoding, true);
    gMW->m_settings.add(ui->sbViewsToolBarSizes);
    gMW->m_settings.add(ui->sbFileListItemSize);
    gMW->m_settings.add(ui->sbViewsTimeDecimals);
    gMW->m_settings.add(ui->cbViewsShowMusicNoteNames);
    gMW->m_settings.add(ui->cbViewsAddMarginsOnSelection);
    gMW->m_settings.add(ui->cbViewsScrollBarsShow);
    gMW->m_settings.addFont(ui->lblGridFontSample);

    adjustSize();
}

void WDialogSettings::settingsSave() {

    // Save all the automatic settings
    gMW->m_settings.saveAll();

    // Save the particular global settings
    gMW->m_settings.setValue("cbPlaybackAudioOutputDevices", ui->cbPlaybackAudioOutputDevices->currentText());

    // Save the particular settings of different widgets
    gMW->m_settings.setValue("m_qxtSpectrogramSpanSlider_lower", gMW->m_qxtSpectrogramSpanSlider->lowerValue());
    gMW->m_settings.setValue("m_qxtSpectrogramSpanSlider_upper", gMW->m_qxtSpectrogramSpanSlider->upperValue());

    QString strMain;
    QList<int> sizeslist = gMW->ui->splitterMain->sizes();
    for(QList<int>::iterator it=sizeslist.begin(); it!=sizeslist.end(); it++)
            strMain += QString::number(*it) + " ";
    gMW->m_settings.setValue("splitterMain", strMain);

    QString strViews;
    sizeslist = gMW->ui->splitterViews->sizes();
    for(QList<int>::iterator it=sizeslist.begin(); it!=sizeslist.end(); it++)
            strViews += QString::number(*it) + " ";
    gMW->m_settings.setValue("splitterViews", strViews);

    QString strSpectra;
    sizeslist = gMW->ui->splitterSpectra->sizes();
    for(QList<int>::iterator it=sizeslist.begin(); it!=sizeslist.end(); it++)
            strSpectra += QString::number(*it) + " ";
    gMW->m_settings.setValue("splitterSpectra", strSpectra);
}
void WDialogSettings::settingsClear() {
    gMW->m_settings.clearAll();
    QMessageBox::information(this, "Reset factory settings", "<p>The settings have been reset to their original values.</p><p>Please restart DFasma</p>");
}


void WDialogSettings::setSBButterworthOrderChangeValue(int order) {
    if(order%2==1)
        ui->sbPlaybackButterworthOrder->setValue(order+1);
}

void WDialogSettings::setCKAvoidClicksAddWindows(bool add) {
    FTSound::sm_playwin_use = add;
    FTSound::setAvoidClicksWindowDuration(ui->sbPlaybackAvoidClicksWindowDuration->value());
    ui->sbPlaybackAvoidClicksWindowDuration->setEnabled(add);
    ui->lblAvoidClicksWindowDurationLabel->setEnabled(add);
}

void WDialogSettings::setSBAvoidClicksWindowDuration(double halfduration) {
    FTSound::setAvoidClicksWindowDuration(halfduration);
}

void WDialogSettings::changeFont() {
    QFontDialog dlg(ui->lblGridFontSample->font(), this);
    dlg.exec();
    ui->lblGridFontSample->setFont(dlg.selectedFont());
}

WDialogSettings::~WDialogSettings() {
    delete ui;
}
