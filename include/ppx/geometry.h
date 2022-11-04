// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ppx_geometry_h
#define ppx_geometry_h

#include "ppx/tri_mesh.h"
#include "ppx/wire_mesh.h"
#include "ppx/grfx/grfx_config.h"

namespace ppx {

enum GeometryVertexAttributeLayout
{
    GEOMETRY_VERTEX_ATTRIBUTE_LAYOUT_INTERLEAVED = 1,
    GEOMETRY_VERTEX_ATTRIBUTE_LAYOUT_PLANAR      = 2,
};

//! @struct GeometryOptions
//!
//! primtiveTopology
//!   - only TRIANGLE_LIST is currently supported
//!
//! indexType
//!   - use UNDEFINED to indicate there's not index data
//!
//! attributeLayout
//!   - if INTERLEAVED only bindings[0] is used
//!   - if PLANAR bindings[0..bindingCount] are used
//!   - if PLANAR bindings can only have 1 attribute
//!
//! Supported vertex semantics:
//!    grfx::VERTEX_SEMANTIC_POSITION
//!    grfx::VERTEX_SEMANTIC_NORMAL
//!    grfx::VERTEX_SEMANTIC_COLOR
//!    grfx::VERTEX_SEMANTIC_TANGENT
//!    grfx::VERTEX_SEMANTIC_BITANGEN
//!    grfx::VERTEX_SEMANTIC_TEXCOORD
//!
struct GeometryOptions
{
    grfx::IndexType               indexType                               = grfx::INDEX_TYPE_UNDEFINED;
    GeometryVertexAttributeLayout vertexAttributeLayout                   = ppx::GEOMETRY_VERTEX_ATTRIBUTE_LAYOUT_INTERLEAVED;
    uint32_t                      vertexBindingCount                      = 0;
    grfx::VertexBinding           vertexBindings[PPX_MAX_VERTEX_BINDINGS] = {};
    grfx::PrimitiveTopology       primtiveTopology                        = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Creates a create info objects with a UINT16 or UINT32 index
    // type and position vertex attribute.
    //
    static GeometryOptions InterleavedU16();
    static GeometryOptions InterleavedU32();
    static GeometryOptions PlanarU16();
    static GeometryOptions PlanarU32();

    // Create a create info with a position vertex attribute.
    //
    static GeometryOptions Interleaved();
    static GeometryOptions Planar();

    GeometryOptions& IndexType(grfx::IndexType indexType_);
    GeometryOptions& IndexTypeU16();
    GeometryOptions& IndexTypeU32();

    // NOTE: Vertex input locations (Vulkan) are based on the order of
    //       when the attribute is added.
    //
    //       Example (assumes no attributes exist):
    //         AddPosition(); // location = 0
    //         AddColor();    // location = 1
    //
    //       Example (2 attributes exist):
    //         AddTexCoord(); // location = 2
    //         AddTangent();  // location = 3
    //
    // WARNING: Changing the attribute layout, index type, or messing
    //          with the vertex bindings after or in between calling
    //          these functions can result in undefined behavior.
    //
    GeometryOptions& AddPosition(grfx::Format format = grfx::FORMAT_R32G32B32_FLOAT);
    GeometryOptions& AddNormal(grfx::Format format = grfx::FORMAT_R32G32B32_FLOAT);
    GeometryOptions& AddColor(grfx::Format format = grfx::FORMAT_R32G32B32_FLOAT);
    GeometryOptions& AddTexCoord(grfx::Format format = grfx::FORMAT_R32G32_FLOAT);
    GeometryOptions& AddTangent(grfx::Format format = grfx::FORMAT_R32G32B32A32_FLOAT);
    GeometryOptions& AddBitangent(grfx::Format format = grfx::FORMAT_R32G32B32_FLOAT);

private:
    GeometryOptions& AddAttribute(grfx::VertexSemantic semantic, grfx::Format format);
};

//! @class Geometry
//!
//! Implementation Notes:
//!   - Do not modify the vertex buffers directly, use the Append* functions
//!
class Geometry
{
public:
    enum BufferType
    {
        BUFFER_TYPE_VERTEX = 1,
        BUFFER_TYPE_INDEX  = 2,
    };

    //! @class Buffer
    //!
    //! Element count is data size divided by element size.
    //! Example:
    //!   - If  buffer is storing 16 bit indices then element size
    //!     is 2 bytes. Dividing the value of GetSize() by 2 will
    //!     return the element count, i.e. the number of indices.
    //!
    //! There's two different ways to use Buffer. Mixing them will most
    //! likely lead to undefined behavior.
    //!
    //! Use #1:
    //!   - Create a buffer object with type and element size
    //!   - Call SetSize() to allocate storage
    //!   - Grab the pointer with GetData() and memcpy to buffer object
    //!
    //! Use #2:
    //!   - Create a buffer object with type and element size
    //!   - Call Append<T>() to append data to it
    //!
    class Buffer
    {
    public:
        Buffer() {}
        Buffer(BufferType type, uint32_t elementSize)
            : mType(type), mElementSize(elementSize) {}
        ~Buffer() {}

        BufferType  GetType() const { return mType; }
        uint32_t    GetElementSize() const { return mElementSize; }
        uint32_t    GetSize() const { return CountU32(mData); }
        void        SetSize(uint32_t size) { mData.resize(size); }
        char*       GetData() { return DataPtr(mData); }
        const char* GetData() const { return DataPtr(mData); }
        uint32_t    GetElementCount() const;

        // Trusts that calling code is well behaved :)
        //
        template <typename T>
        void Append(const T& value)
        {
            uint32_t sizeOfValue = static_cast<uint32_t>(sizeof(T));

            // Current size
            size_t offset = mData.size();
            // Allocate storage for incoming data
            mData.resize(offset + sizeOfValue);
            // Copy data
            const void* pSrc = &value;
            void*       pDst = mData.data() + offset;
            memcpy(pDst, pSrc, sizeOfValue);
        }

    private:
        BufferType        mType        = BUFFER_TYPE_VERTEX;
        uint32_t          mElementSize = 0;
        std::vector<char> mData;
    };

    // ---------------------------------------------------------------------------------------------
public:
    Geometry() {}
    virtual ~Geometry() {}

private:
    Result InternalCtor();

public:
    // Create object using parameters from createInfo
    static Result Create(const GeometryOptions& createInfo, Geometry* pGeometry);

    // Create object using parameters from createInfo using data from mesh
    static Result Create(
        const GeometryOptions& createInfo,
        const TriMesh&         mesh,
        Geometry*              pGeometry);

    // Create object using parameters from createInfo using data from tri mesh
    static Result Create(
        const GeometryOptions& createInfo,
        const WireMesh&        mesh,
        Geometry*              pGeometry);

    // Create object with a create info derived from mesh
    static Result Create(const TriMesh& mesh, Geometry* pGeometry);
    static Result Create(const WireMesh& mesh, Geometry* pGeometry);

    grfx::IndexType         GetIndexType() const { return mCreateInfo.indexType; }
    const Geometry::Buffer* GetIndexBuffer() const { return &mIndexBuffer; }
    uint32_t                GetIndexCount() const;

    GeometryVertexAttributeLayout GetVertexAttributeLayout() const { return mCreateInfo.vertexAttributeLayout; }
    uint32_t                      GetVertexBindingCount() const { return mCreateInfo.vertexBindingCount; }
    const grfx::VertexBinding*    GetVertexBinding(uint32_t index) const;

    uint32_t                GetVertexCount() const;
    uint32_t                GetVertexBufferCount() const { return CountU32(mVertexBuffers); }
    const Geometry::Buffer* GetVertexBuffer(uint32_t index) const;
    uint32_t                GetLargestBufferSize() const;

    // Appends triangle or edge vertex indices to index buffer
    //
    // Will cast to uint16_t if geometry index type is UINT16.
    // NOOP if index type is UNDEFINED (geometry does not have index data).
    //
    void AppendIndicesTriangle(uint32_t vtx0, uint32_t vtx1, uint32_t vtx2);
    void AppendIndicesEdge(uint32_t vtx0, uint32_t vtx1);

    // Append multiple attributes at once
    //
    uint32_t AppendVertexData(const TriMeshVertexData& vtx);
    uint32_t AppendVertexData(const WireMeshVertexData& vtx);

    // Appends triangle or edge vertex data and indices (if present)
    //
    //
    void AppendTriangle(const TriMeshVertexData& vtx0, const TriMeshVertexData& vtx1, const TriMeshVertexData& vtx2);
    void AppendEdge(const WireMeshVertexData& vtx0, const WireMeshVertexData& vtx1);

    // Append individual attributes
    //
    // For attribute layout GEOMETRY_ATTRIBUTE_LAYOUT_PLANAR only, will NOOP
    // otherwise.
    //
    // Missing attributes will also result in NOOP. For instance if a geomtry
    // object was not created with color, calling AppendColor() will NOOP.
    //
    uint32_t AppendPosition(const float3& value);
    void     AppendNormal(const float3& value);
    void     AppendColor(const float3& value);
    void     AppendTexCoord(const float2& value);
    void     AppendTangent(const float4& value);
    void     AppendBitangent(const float3& value);

private:
    uint32_t AppendVertexInterleaved(const TriMeshVertexData& vtx);
    uint32_t AppendVertexInterleaved(const WireMeshVertexData& vtx);

private:
    GeometryOptions               mCreateInfo = {};
    Geometry::Buffer              mIndexBuffer;
    std::vector<Geometry::Buffer> mVertexBuffers;
    uint32_t                      mPositionBufferIndex  = PPX_VALUE_IGNORED;
    uint32_t                      mNormaBufferIndex     = PPX_VALUE_IGNORED;
    uint32_t                      mColorBufferIndex     = PPX_VALUE_IGNORED;
    uint32_t                      mTexCoordBufferIndex  = PPX_VALUE_IGNORED;
    uint32_t                      mTangentBufferIndex   = PPX_VALUE_IGNORED;
    uint32_t                      mBitangentBufferIndex = PPX_VALUE_IGNORED;
};

} // namespace ppx

#endif // ppx_geometry_h
