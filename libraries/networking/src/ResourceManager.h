//
//  ResourceManager.h
//
//  Created by Ryan Huffman on 2015/07/23
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ResourceManager_h
#define hifi_ResourceManager_h

#include <functional>

#include "ResourceRequest.h"

class ResourceManager {
public:
    static ResourceRequest* createResourceRequest(QObject* parent, const QUrl& url);
};

#endif
