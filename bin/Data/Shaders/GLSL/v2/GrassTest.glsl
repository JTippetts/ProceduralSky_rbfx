#define URHO3D_CUSTOM_MATERIAL_UNIFORMS
#define URHO3D_DISABLE_SPECULAR_SAMPLING
#define URHO3D_DISABLE_EMISSIVE_SAMPLING

#include "_Config.glsl"
#include "_Uniforms.glsl"

UNIFORM_BUFFER_BEGIN(4, Material)
    DEFAULT_MATERIAL_UNIFORMS
	UNIFORM(vec4 cHeightMapData)
	UNIFORM(float cLayerY)
UNIFORM_BUFFER_END(4, Material)

uniform sampler2D sHeightMap2;

VERTEX_OUTPUT_HIGHP(vec2 vHeightMapCoords)

#include "_Material.glsl"

#ifdef URHO3D_VERTEX_SHADER
void main()
{
    VertexTransform vertexTransform = GetVertexTransform();
 
	
	vec2 t=vertexTransform.position.xz / cHeightMapData.z;
	vec2 htuv=vec2((t.x/cHeightMapData.x)+0.5, 1.0-((t.y/cHeightMapData.y)+0.5));
	vec4 htt=textureLod(sHeightMap2, htuv, 0.0);
	float htscale=cHeightMapData.w*255.0;
	float ht=htt.r*htscale + htt.g*cHeightMapData.w;
	float y=(vertexTransform.position.y - cLayerY) + ht;
	vertexTransform.position.y=y;
	
	FillVertexOutputs(vertexTransform);
}
#endif

#ifdef URHO3D_PIXEL_SHADER
void main()
{
#ifdef URHO3D_DEPTH_ONLY_PASS
    DefaultPixelShader();
#else
    SurfaceData surfaceData;

    FillSurfaceCommon(surfaceData);
    FillSurfaceNormal(surfaceData);
    FillSurfaceMetallicRoughnessOcclusion(surfaceData);
    FillSurfaceReflectionColor(surfaceData);
    FillSurfaceBackground(surfaceData);
    FillSurfaceAlbedoSpecular(surfaceData);
    FillSurfaceEmission(surfaceData);
	
    half3 finalColor = GetFinalColor(surfaceData);
    gl_FragColor.rgb = ApplyFog(finalColor, surfaceData.fogFactor);
    gl_FragColor.a = GetFinalAlpha(surfaceData);
#endif
}
#endif