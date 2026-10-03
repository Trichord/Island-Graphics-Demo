// Types
struct SourceDrawElementsIndirectCommand
{
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    int baseVertex;
    unsigned int baseInstance;
    unsigned int drawBucket;
    unsigned int drawCommandsBaseIndex;
};

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