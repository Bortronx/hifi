//
//  Agent.cpp
//  hifi
//
//  Created by Stephen Birarda on 7/1/13.
//  Copyright (c) 2013 HighFidelity, Inc. All rights reserved.
//

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <AvatarData.h>
#include <NodeList.h>
#include <PacketHeaders.h>
#include <UUID.h>
#include <VoxelConstants.h>

#include "ScriptEngine.h"

ScriptEngine::ScriptEngine(QString scriptContents) {
    _scriptContents = scriptContents;
    _isFinished = false;
}

void ScriptEngine::run() {
    QScriptEngine engine;
    
    _voxelScriptingInterface.init();
    _particleScriptingInterface.init();
    
    // register meta-type for glm::vec3 conversions
    registerMetaTypes(&engine);
    
    QScriptValue agentValue = engine.newQObject(this);
    engine.globalObject().setProperty("Agent", agentValue);
    
    QScriptValue voxelScripterValue =  engine.newQObject(&_voxelScriptingInterface);
    engine.globalObject().setProperty("Voxels", voxelScripterValue);

    QScriptValue particleScripterValue =  engine.newQObject(&_particleScriptingInterface);
    engine.globalObject().setProperty("Particles", particleScripterValue);
    
    QScriptValue treeScaleValue = engine.newVariant(QVariant(TREE_SCALE));
    engine.globalObject().setProperty("TREE_SCALE", treeScaleValue);
    
    const unsigned int VISUAL_DATA_CALLBACK_USECS = (1.0 / 60.0) * 1000 * 1000;
    
    // let the VoxelPacketSender know how frequently we plan to call it
    _voxelScriptingInterface.getVoxelPacketSender()->setProcessCallIntervalHint(VISUAL_DATA_CALLBACK_USECS);
    _particleScriptingInterface.getParticlePacketSender()->setProcessCallIntervalHint(VISUAL_DATA_CALLBACK_USECS);

    qDebug() << "Script:\n" << _scriptContents << "\n";
    
    QScriptValue result = engine.evaluate(_scriptContents);
    qDebug() << "Evaluated script.\n";
    
    if (engine.hasUncaughtException()) {
        int line = engine.uncaughtExceptionLineNumber();
        qDebug() << "Uncaught exception at line" << line << ":" << result.toString() << "\n";
    }
    
    timeval startTime;
    gettimeofday(&startTime, NULL);
    
    int thisFrame = 0;

    qDebug() << "before while... thisFrame:" << thisFrame << "\n";
    
    while (!_isFinished) {

        qDebug() << "while... thisFrame:" << thisFrame << "\n";
        
        int usecToSleep = usecTimestamp(&startTime) + (thisFrame++ * VISUAL_DATA_CALLBACK_USECS) - usecTimestampNow();
        if (usecToSleep > 0) {
            usleep(usecToSleep);
        }
        
        QCoreApplication::processEvents();
        
        bool willSendVisualDataCallBack = false;
        if (_voxelScriptingInterface.getVoxelPacketSender()->serversExist()) {            
            // allow the scripter's call back to setup visual data
            willSendVisualDataCallBack = true;
            
            // release the queue of edit voxel messages.
            _voxelScriptingInterface.getVoxelPacketSender()->releaseQueuedMessages();
            
            // since we're in non-threaded mode, call process so that the packets are sent
            //_voxelScriptingInterface.getVoxelPacketSender()->process();
        }

        if (_particleScriptingInterface.getParticlePacketSender()->serversExist()) {
            // allow the scripter's call back to setup visual data
            willSendVisualDataCallBack = true;
            
            // release the queue of edit voxel messages.
            _particleScriptingInterface.getParticlePacketSender()->releaseQueuedMessages();
            
            // since we're in non-threaded mode, call process so that the packets are sent
            //_particleScriptingInterface.getParticlePacketSender()->process();
        }
        
        if (willSendVisualDataCallBack) {
            qDebug() << "willSendVisualDataCallback thisFrame:" << thisFrame << "\n";
            emit willSendVisualDataCallback();
        }

        
        if (engine.hasUncaughtException()) {
            int line = engine.uncaughtExceptionLineNumber();
            qDebug() << "Uncaught exception at line" << line << ":" << engine.uncaughtException().toString() << "\n";
        }
    }
    emit finished();
}
