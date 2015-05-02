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

#include "ftfzero.h"

#include <iostream>
using namespace std;

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
using namespace Easdif;
#endif

#include <QFileInfo>
#include <QMessageBox>
#include <qmath.h>
#include <qendian.h>

#include "wmainwindow.h"
#include "ui_wdialogsettings.h"

void FTFZero::init(){

}

FTFZero::FTFZero(const QString& _fileName, QObject *parent)
    : FileType(FTFZERO, _fileName, this)
{
    Q_UNUSED(parent)

    init();

    if(!fileFullPath.isEmpty()){
        checkFileStatus(CFSMEXCEPTION);
        load();
    }

    gMW->ftfzeros.push_back(this);
}

FTFZero::FTFZero(const FTFZero& ft)
    : QObject(ft.parent())
    , FileType(FTFZERO, ft.fileFullPath, this)
{
    init();

    ts = ft.ts;
    f0s = ft.f0s;

    m_lastreadtime = ft.m_lastreadtime;
    m_modifiedtime = ft.m_modifiedtime;

    gMW->ftfzeros.push_back(this);
}

FileType* FTFZero::duplicate(){
    return new FTFZero(*this);
}

void FTFZero::load() {

    // TODO load text files

    #ifdef SUPPORT_SDIF

        SDIFEntity readentity;

        try {
            if (!readentity.OpenRead(fileFullPath.toLocal8Bit().constData()) )
                throw QString("SDIF: Cannot open file");
        }
        catch(SDIFBadHeader& e) {
            e.ErrorMessage();
            throw QString("SDIF: bad header");
        }

        readentity.ChangeSelection("/1FQ0.1_1"); // Select directly the f0 values

        SDIFFrame frame;
        try{
            while (1) {

                /* reading the next frame of the EntityRead, return the number of
                bytes read, return 0 if the frame is not selected
                and produces an exception if eof is reached */
                if(!readentity.ReadNextFrame(frame))
                    continue;

                double t = frame.GetTime();

                for (unsigned int i=0 ; i < frame.GetNbMatrix() ; i++) {
                    /*take the matrix number "i" and put it in tmpMatrix */
                    SDIFMatrix tmpMatrix = frame.GetMatrix(i);

                    if(tmpMatrix.GetNbCols()<1 || tmpMatrix.GetNbRows()<1)
                        throw QString("f0 value is missing in a 1FQ0 frame at time ")+QString::number(t);

                    ts.push_back(t);
                    f0s.push_back(tmpMatrix.GetDouble(0, 0));
    //                cout << "VALUES " << *(ts.end()) << ":" << *(f0s.end()) << endl;

    //                /* if you want to access to the data : an example, if we want
    //                 to multiply with 2 the last column of a matrix : */
    //                double dou;
    //                int ncols = tmpMatrix.GetNbCols();
    //                int nrows = tmpMatrix.GetNbRows();
    //                cout << "nrows=" << nrows << " ncols=" << ncols << endl;
    //                for(int i = 0 ; i < nrows ; i++) {
    //                    for(int j = 0 ; j < ncols ; j++) {
    //                        dou = tmpMatrix.GetDouble(i, j);
    //                        cout << dou << " ";
    //                    }
    //                }
    //                cout << endl;
                }

                frame.ClearData();
            }
        }
        catch(SDIFEof& e) {
        }
        catch(SDIFUnDefined& e) {
            e.ErrorMessage();
            throw QString("SDIF: ")+e.what();
        }
        catch(Easdif::SDIFException&e) {
            e.ErrorMessage();
            throw QString("SDIF: ")+e.what();
        }
        catch(std::exception &e) {
            throw QString("SDIF: ")+e.what();
        }
        catch(QString &e) {
            throw QString("SDIF: ")+e;
        }
    #endif

    m_lastreadtime = QDateTime::currentDateTime();
    setStatus();
}

void FTFZero::reload() {
//    cout << "FTFZero::reload" << endl;

    if(!checkFileStatus(CFSMMESSAGEBOX))
        return;

    // Reset everything ...
    ts.clear();
    f0s.clear();

    // ... and reload the data from the file
    load();
}

QString FTFZero::info() const {
    QString str = FileType::info();
    str += "Number of f0 values: " + QString::number(ts.size()) + "<br/>";
    if(ts.size()>0){
        // TODO Should be done once
        double meandts = 0.0;
        double meanf0 = f0s[0];
        double f0min = f0s[0];
        double f0max = f0s[0];
        for(size_t i=1; i<ts.size(); ++i){
            f0min = std::min(f0min, f0s[i]);
            f0max = std::max(f0max, f0s[i]);
            meandts += ts[i]-ts[i-1];
            meanf0 += f0s[i];
        }
        meandts /= ts.size();
        meanf0 /= f0s.size();
        str += "Average sampling: " + QString("%1").arg(meandts, 0,'f',gMW->m_dlgSettings->ui->spViewTimeDecimals->value()) + "s<br/>";
        str += QString("F0 in [%1,%2]Hz<br/>").arg(f0min, 0,'g',3).arg(f0max, 0,'g',5);
        str += QString("Mean f0=%3Hz").arg(meanf0, 0,'g',5);
    }
    return str;
}

double FTFZero::getLastSampleTime() const {
    if(ts.empty())
        return 0.0;
    else
        return *((ts.end()-1));
}

FTFZero::~FTFZero() {
}
