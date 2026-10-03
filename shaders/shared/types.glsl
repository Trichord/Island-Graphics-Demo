// Types
struct DrawElementsIndirectCommand
{
    uint count;
    uint instanceCount;
    uint firstIndex;
    int baseVertex;
    uint baseInstance;
};

struct Metadata
{
	int albedoArray;
	int albedoLayer;
	int armArray;
	int armLayer;
	int normalArray;
	int normalLayer;
	int cubemapArray;
	int cubemapLayer;
	uint visibleInstancesBaseIndex;
};

struct BufferMap
{
    uint sharedIndex;
    uint drawBucket;
    uint drawCommandsBaseIndex;
    uint visibleInstancesBaseIndex;
};

struct AABB
{
    vec4 center;
    vec4 extent;
};

struct VertexSkin
{
    vec4 weights;
    uvec4 boneIDs;
};