/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
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

#ifndef _U2_NEIGHBOR_JOIN_ADAPTER_H_
#define _U2_NEIGHBOR_JOIN_ADAPTER_H_

#include <QObject>

#include <U2Algorithm/PhyTreeGenerator.h>
#include <U2Algorithm/PhyTreeGeneratorTask.h>
//#include <U2Algorithm/SubstMatrixRegistry.h>

#include <U2Core/AppResources.h>
//#include <U2Core/PhyTree.h>

//#include <U2View/CreatePhyTreeDialogController.h>

namespace U2 { 

class MAlignment;
class TaskStateInfo;
class PhyTreeGeneratorTask;

class NeighborJoinAdapter : public PhyTreeGenerator {
public:
    Task * createCalculatePhyTreeTask(const MAlignment& ma, const CreatePhyTreeSettings& s);
    CreatePhyTreeWidget *createPhyTreeSettingsWidget(const MAlignment &ma, QWidget *parent = NULL);
};

class NeighborJoinCalculateTreeTask: public PhyTreeGeneratorTask {
public:
    NeighborJoinCalculateTreeTask(const MAlignment &ma, const CreatePhyTreeSettings &s);
    void run();

private:
    static QMutex runLock;
    MemoryLocker memLocker;
};

}   // namespace U2

#endif // _U2_NEIGHBOR_JOIN_ADAPTER_H_
