//
//  HttpManager.cpp
//  hifi
//
//  Created by Stephen Birarda on 1/16/14.
//  Copyright (c) 2014 HighFidelity, Inc. All rights reserved.
//
//  Heavily based on Andrzej Kapolka's original HttpManager class
//  found from another one of his projects.
//  (https://github.com/ey6es/witgap/tree/master/src/cpp/server/http)
//

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeDatabase>
#include <QtNetwork/QTcpSocket>

#include "HttpConnection.h"
#include "HttpManager.h"

bool HttpManager::handleRequest(HttpConnection* connection, const QString& path) {
    QString subPath = path;
    
    // remove any slash at the beginning of the path
    if (subPath.startsWith('/')) {
        subPath.remove(0, 1);
    }
    
    QString filePath;
    
    // if the last thing is a trailing slash then we want to look for index file
    if (subPath.endsWith('/') || subPath.size() == 0) {
        QStringList possibleIndexFiles = QStringList() << "index.html" << "index.shtml";
        
        foreach (const QString& possibleIndexFilename, possibleIndexFiles) {
            if (QFileInfo(_documentRoot + subPath + possibleIndexFilename).exists()) {
                filePath = _documentRoot + subPath + possibleIndexFilename;
                break;
            }
        }
    } else if (QFileInfo(_documentRoot + subPath).isFile()) {
        filePath = _documentRoot + subPath;
    }

    
    if (!filePath.isEmpty()) {
        static QMimeDatabase mimeDatabase;
        
        QFile localFile(filePath);
        localFile.open(QIODevice::ReadOnly);
        QString localFileString(localFile.readAll());
        
        QFileInfo localFileInfo(filePath);
        
        if (localFileInfo.completeSuffix() == "shtml") {
            // this is a file that may have some SSI statements
            // the only thing we support is the include directive, but check the contents for that
            
            // setup our static QRegExp that will catch <!--#include virtual ... --> and <!--#include file .. --> directives
            const QString includeRegExpString = "<!--\\s*#include\\s+(virtual|file)\\s?=\\s?\"(\\S+)\"\\s*-->";
            QRegExp includeRegExp(includeRegExpString);
            
            int matchPosition = 0;
            
            
            while ((matchPosition = includeRegExp.indexIn(localFileString, matchPosition)) != -1) {
                // check if this is a file or vitual include
                bool isFileInclude = includeRegExp.cap(1) == "file";
                
                // setup the correct file path for the included file
                QString includeFilePath = isFileInclude
                    ? localFileInfo.canonicalPath() + "/" + includeRegExp.cap(2)
                    : _documentRoot + includeRegExp.cap(2);
                
                QString replacementString;
                
                if (QFileInfo(includeFilePath).isFile()) {
                    
                    QFile includedFile(includeFilePath);
                    includedFile.open(QIODevice::ReadOnly);
                    
                    replacementString = QString(includedFile.readAll());
                } else {
                    qDebug() << "SSI include directive referenced a missing file:" << includeFilePath;
                }
                
                // replace the match with the contents of the file, or an empty string if the file was not found
                localFileString.replace(matchPosition, includeRegExp.matchedLength(), replacementString);
                
                // push the match position forward so we can check the next match
                matchPosition += includeRegExp.matchedLength();
            }
        }
        
        connection->respond("200 OK", localFileString.toLocal8Bit(), qPrintable(mimeDatabase.mimeTypeForFile(filePath).name()));
    } else {
        // respond with a 404
        connection->respond("404 Not Found", "Resource not found.");
    }
    
    return true;
}

HttpManager::HttpManager(quint16 port, const QString& documentRoot, QObject* parent) :
    QTcpServer(parent),
    _documentRoot(documentRoot) {
    // start listening on the passed port
    if (!listen(QHostAddress("0.0.0.0"), port)) {
        qDebug() << "Failed to open HTTP server socket:" << errorString();
        return;
    }
    
    // connect the connection signal
    connect(this, SIGNAL(newConnection()), SLOT(acceptConnections()));
}

void HttpManager::acceptConnections() {
    QTcpSocket* socket;
    while ((socket = nextPendingConnection()) != 0) {
        new HttpConnection(socket, this);
    }
}
