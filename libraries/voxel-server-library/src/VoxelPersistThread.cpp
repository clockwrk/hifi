//
//  VoxelPersistThread.cpp
//  voxel-server
//
//  Created by Brad Hefta-Gaub on 8/21/13
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//
//  Threaded or non-threaded voxel persistence
//

#include <QDebug>
#include <NodeList.h>
#include <PerfStat.h>
#include <SharedUtil.h>

#include "VoxelPersistThread.h"
#include "VoxelServer.h"

VoxelPersistThread::VoxelPersistThread(VoxelTree* tree, const char* filename, int persistInterval) :
    _tree(tree),
    _filename(filename),
    _persistInterval(persistInterval),
    _initialLoad(false) {
}

bool VoxelPersistThread::process() {

    if (!_initialLoad) {
        _initialLoad = true;
        qDebug("loading voxels from file: %s...\n", _filename);

        qDebug("sizeof(VoxelNode)=%d sizeof(AABox)=%d sizeof(oldAABox)=%d\n", sizeof(VoxelNode), sizeof(AABox), sizeof(oldAABox));

        bool persistantFileRead = _tree->readFromSVOFile(_filename);
        if (persistantFileRead) {
            PerformanceWarning warn(true, "reaverageVoxelColors()", true);
            
            // after done inserting all these voxels, then reaverage colors
            _tree->reaverageVoxelColors(_tree->rootNode);
            qDebug("Voxels reAveraged\n");
        }
        
        _tree->clearDirtyBit(); // the tree is clean since we just loaded it
        qDebug("DONE loading voxels from file... fileRead=%s\n", debug::valueOf(persistantFileRead));
        
        unsigned long nodeCount = VoxelNode::getNodeCount();
        unsigned long internalNodeCount = VoxelNode::getInternalNodeCount();
        unsigned long leafNodeCount = VoxelNode::getLeafNodeCount();
        qDebug("Nodes after loading scene %lu nodes %lu internal %lu leaves\n", nodeCount, internalNodeCount, leafNodeCount);
    }
    
    uint64_t MSECS_TO_USECS = 1000;
    usleep(_persistInterval * MSECS_TO_USECS);


    // check the dirty bit and persist here...
    if (_tree->isDirty()) {
        qDebug("saving voxels to file %s...\n",_filename);
        _tree->writeToSVOFile(_filename);
        _tree->clearDirtyBit(); // tree is clean after saving
        qDebug("DONE saving voxels to file...\n");
    }

    return isStillRunning();  // keep running till they terminate us
}
