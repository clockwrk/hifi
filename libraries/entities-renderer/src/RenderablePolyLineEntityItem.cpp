//
//  RenderablePolyLineEntityItem.cpp
//  libraries/entities-renderer/src/
//
//  Created by Eric Levin on 6/22/15
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <glm/gtx/quaternion.hpp>

#include <gpu/GPUConfig.h>
#include <GeometryCache.h>
#include <TextureCache.h>
#include <PathUtils.h>
#include <DeferredLightingEffect.h>
#include <PerfStat.h>

#include "RenderablePolyLineEntityItem.h"

#include "paintStroke_vert.h"
#include "paintStroke_frag.h"



EntityItemPointer RenderablePolyLineEntityItem::factory(const EntityItemID& entityID, const EntityItemProperties& properties) {
    return EntityItemPointer(new RenderablePolyLineEntityItem(entityID, properties));
}

RenderablePolyLineEntityItem::RenderablePolyLineEntityItem(const EntityItemID& entityItemID, const EntityItemProperties& properties) :
PolyLineEntityItem(entityItemID, properties) {
    _numVertices = 0;

}

gpu::PipelinePointer RenderablePolyLineEntityItem::_pipeline;
gpu::Stream::FormatPointer RenderablePolyLineEntityItem::_format;
gpu::TexturePointer RenderablePolyLineEntityItem::_texture;
GLint RenderablePolyLineEntityItem::PAINTSTROKE_GPU_SLOT;

void RenderablePolyLineEntityItem::createPipeline() {
    static const int NORMAL_OFFSET = 12;
//    static const int COLOR_OFFSET = 24;
    static const int TEXTURE_OFFSET = 24;
    
    auto textureCache = DependencyManager::get<TextureCache>();
    QString path = PathUtils::resourcesPath() + "images/paintStroke.png";
//    QString path = PathUtils::resourcesPath() + "images/arrow.png";
    qDebug() << "IMAGE PATHHHH ******: " << path;
    _texture = textureCache->getImageTexture(path);
//    _texture = textureCache->getBlueTexture();
    _format.reset(new gpu::Stream::Format());
    _format->setAttribute(gpu::Stream::POSITION, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), 0);
    _format->setAttribute(gpu::Stream::NORMAL, 0, gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ), NORMAL_OFFSET);
//    _format->setAttribute(gpu::Stream::COLOR, 0, gpu::Element(gpu::VEC4, gpu::UINT8, gpu::RGBA), COLOR_OFFSET);
    _format->setAttribute(gpu::Stream::TEXCOORD, 0, gpu::Element(gpu::VEC2, gpu::FLOAT, gpu::UV), TEXTURE_OFFSET);
    
    auto VS = gpu::ShaderPointer(gpu::Shader::createVertex(std::string(paintStroke_vert)));
    auto PS = gpu::ShaderPointer(gpu::Shader::createPixel(std::string(paintStroke_frag)));
    gpu::ShaderPointer program = gpu::ShaderPointer(gpu::Shader::createProgram(VS, PS));
    
    gpu::Shader::BindingSet slotBindings;
    PAINTSTROKE_GPU_SLOT = 0;
    slotBindings.insert(gpu::Shader::Binding(std::string("paintStrokeTextureBinding"), PAINTSTROKE_GPU_SLOT));
    gpu::Shader::makeProgram(*program, slotBindings);
    
    gpu::StatePointer state = gpu::StatePointer(new gpu::State());
    state->setDepthTest(true, true, gpu::LESS_EQUAL);
    state->setBlendFunction(true,
                            gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
                            gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
   _pipeline = gpu::PipelinePointer(gpu::Pipeline::create(program, state));
}

void RenderablePolyLineEntityItem::updateGeometry() {
    _numVertices = 0;
    _verticesBuffer.reset(new gpu::Buffer());
    int vertexIndex = 0;
    vec2 uv;
    float tailStart = 0.0;
    float tailEnd = 0.25;
    float tailLength = tailEnd - tailStart;
    
    float headStart = 0.76;
    float headEnd = 1.0;
    float headLength = headEnd - headStart;
    float uCoord, vCoord;
    
    int numTailStrips =  5;
    int numHeadStrips = 10;
    int startHeadIndex = _normals.size() - numHeadStrips;
    for (int i = 0; i < _normals.size(); i++) {
        uCoord = 0.26;
        vCoord = 0;
        //tail
        if(i < numTailStrips) {
            uCoord = float(i)/numTailStrips * tailLength + tailStart;
        }
        
        //head
        if( i > startHeadIndex) {
            uCoord = float(i - startHeadIndex)/numHeadStrips * headLength + headStart;
            qDebug()<< "ucoord:" << uCoord;
        }
        uv = vec2(uCoord, vCoord);
     
        _verticesBuffer->append(sizeof(glm::vec3), (const gpu::Byte*)&_vertices.at(vertexIndex));
        _verticesBuffer->append(sizeof(glm::vec3), (const gpu::Byte*)&_normals.at(i));
//        _verticesBuffer->append(sizeof(int), (gpu::Byte*)&_color);
        _verticesBuffer->append(sizeof(glm::vec2), (gpu::Byte*)&uv);
        vertexIndex++;

        
        uv.y = 1.0;
        _verticesBuffer->append(sizeof(glm::vec3), (const gpu::Byte*)&_vertices.at(vertexIndex));
        _verticesBuffer->append(sizeof(glm::vec3), (const gpu::Byte*)&_normals.at(i));
//        _verticesBuffer->append(sizeof(int), (gpu::Byte*)_color);
        _verticesBuffer->append(sizeof(glm::vec2), (const gpu::Byte*)&uv);
        vertexIndex++;
        
        _numVertices +=2;
    }
    _pointsChanged = false;
    
    _pointsChanged = false;
    
}

namespace render {
    template <> const ItemKey payloadGetKey(const RenderableEntityItemProxy::Pointer& payload) {
        if (payload && payload->entity) {
            if (payload->entity->getType() == EntityTypes::Light) {
                return ItemKey::Builder::light();
            }
        }
        return ItemKey::Builder::transparentShape();
    }
    
    template <> const Item::Bound payloadGetBound(const RenderableEntityItemProxy::Pointer& payload) {
        if (payload && payload->entity) {
            return payload->entity->getAABox();
        }
        return render::Item::Bound();
    }
    template <> void payloadRender(const RenderableEntityItemProxy::Pointer& payload, RenderArgs* args) {
        if (args) {
            if (payload && payload->entity && payload->entity->getVisible()) {
                payload->entity->render(args);
            }
        }
    }
}



void RenderablePolyLineEntityItem::render(RenderArgs* args) {
    QWriteLocker lock(&_quadReadWriteLock);
    if (_points.size() < 2  || _vertices.size() != _normals.size() * 2) {
        return;
    }
    
    if (!_pipeline) {
        createPipeline();
    }
 
    PerformanceTimer perfTimer("RenderablePolyLineEntityItem::render");
    Q_ASSERT(getType() == EntityTypes::PolyLine);
    
    Q_ASSERT(args->_batch);
    if (_pointsChanged) {
        updateGeometry();
    }
    
    gpu::Batch& batch = *args->_batch;
    Transform transform = Transform();
    transform.setTranslation(getPosition());
    transform.setRotation(getRotation());
    batch.setModelTransform(transform);

    batch.setPipeline(_pipeline);
    batch.setResourceTexture(PAINTSTROKE_GPU_SLOT, _texture);

    
    batch.setInputFormat(_format);
    batch.setInputBuffer(0, _verticesBuffer, 0, _format->getChannels().at(0)._stride);
     
    batch.draw(gpu::TRIANGLE_STRIP, _numVertices, 0);
    
    RenderableDebugableEntityItem::render(this, args);
};
