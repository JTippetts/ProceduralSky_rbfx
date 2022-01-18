#define URHO3D_CUSTOM_MATERIAL_UNIFORMS
#define URHO3D_DISABLE_SPECULAR_SAMPLING
#define URHO3D_DISABLE_EMISSIVE_SAMPLING

#define URHO3D_PIXEL_NEED_EYE_VECTOR

#include "_Config.glsl"
#include "_Uniforms.glsl"

UNIFORM_BUFFER_BEGIN(4, Material)
    DEFAULT_MATERIAL_UNIFORMS
	UNIFORM(vec4 cHeightMapData)
	// x: HeightMap width y: Heightmap height z: Height map xz spacing w: Heightmap y spacing
	UNIFORM(vec2 cRadius)
	// x: Outside radius y: Inside radius
	UNIFORM(vec4 cCoverageFactor)
	// Dot with coverage map texture
	UNIFORM(vec3 cCoverageParams)
	// x: Speckling/Sparsity  y: HeightVariance  z: Cell Size
	UNIFORM(vec2 cCoverageFade)
	// x: Low y: High
	
	//UNIFORM(float cSpeckleFactor)
	//UNIFORM(float cHeightVariance)
UNIFORM_BUFFER_END(4, Material)

uniform sampler2D sHeightMap2;
uniform sampler2D sCoverageMap3;

VERTEX_OUTPUT_HIGHP(vec2 vHeightMapCoords)
VERTEX_OUTPUT_HIGHP(float vScaleValue)

#include "_Material.glsl"

#ifdef URHO3D_VERTEX_SHADER

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

vec4 FAST_32_hash( vec2 gridcell )
{
	//    gridcell is assumed to be an integer coordinate
	const vec2 OFFSET = vec2( 26.0, 161.0 );
	const float DOMAIN = 71.0;
	const float SOMELARGEFLOAT = 951.135664;
	vec4 P = vec4( gridcell.xy, gridcell.xy + 1.0.xx );
	P = P - floor(P * ( 1.0 / DOMAIN )) * DOMAIN;    //    truncate the domain
	P += OFFSET.xyxy;                                //    offset to interesting part of the noise
	P *= P;                                          //    calculate and return the hash
	return fract( P.xzxz * P.yyww * ( 1.0 / SOMELARGEFLOAT.x ).xxxx );
}

void main()
{
    VertexTransform vertexTransform = GetVertexTransform();
	
	mat4 modelMatrix = GetModelMatrix();
	vec2 cell=floor(vertexTransform.position.xz*1/cCoverageParams.z);
	vec4 hash=FAST_32_hash(cell);
	
	// Randomized rotation within cell
	mat4 rot=rotationMatrix(vec3(0,1,0), hash.x*6.283);
	
	// Randomized xz offset within cell
	rot[0][3]=sin(hash.w*6.28)*0.5*cCoverageParams.z;
	rot[2][3]=cos(hash.w*6.28)*0.5*cCoverageParams.z;
	
	vertexTransform.position = iPos * rot * modelMatrix;
	
	vec2 d=vertexTransform.position.xz - cCameraPos.xz;
	float dist=length(d);
	dist=(dist-cRadius.y)/(cRadius.x-cRadius.y);
	dist=clamp(dist,0.0,1.0);
	
	vec2 t=vertexTransform.position.xz / cHeightMapData.z;
	vec2 htuv=vec2((t.x/cHeightMapData.x)+0.5, 1.0-((t.y/cHeightMapData.y)+0.5));
	vec4 htt=textureLod(sHeightMap2, htuv, 0.0);
	float ht=(htt.r*255.0 + htt.g) * cHeightMapData.w;
	
	float covscale=smoothstep(cCoverageFade.x, cCoverageFade.y, dot(textureLod(sCoverageMap3, htuv, 0.0), cCoverageFactor));
	//float covscale=step(0.4, dot(textureLod(sCoverageMap3, htuv, 0.0), cCoverageFactor));
	float y=vertexTransform.position.y*dist*covscale*(1+cCoverageParams.y*hash.y)*step(cCoverageParams.x, hash.z) + ht - 0.25;

	vertexTransform.position.y=y;
	vScaleValue=dist*covscale;
	
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
	
	if(vScaleValue<=0.00001) discard;
	if(surfaceData.albedo.a<0.5) discard;
	
    half3 finalColor = GetFinalColor(surfaceData);
    gl_FragColor.rgb = ApplyFog(finalColor, surfaceData.fogFactor);
    gl_FragColor.a = GetFinalAlpha(surfaceData);
#endif
}
#endif