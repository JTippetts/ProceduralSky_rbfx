#define URHO3D_CUSTOM_MATERIAL_UNIFORMS
#define URHO3D_DISABLE_SPECULAR_SAMPLING
#define URHO3D_DISABLE_EMISSIVE_SAMPLING

#define URHO3D_PIXEL_NEED_EYE_VECTOR

#include "_Config.glsl"
#include "_Uniforms.glsl"

UNIFORM_BUFFER_BEGIN(4, Material)
    DEFAULT_MATERIAL_UNIFORMS
	UNIFORM(vec4 cHeightMapData)
	UNIFORM(float cLayerY)
	UNIFORM(vec2 cRadius)
UNIFORM_BUFFER_END(4, Material)

uniform sampler2D sHeightMap2;
uniform sampler2D sCoverageMap3;

VERTEX_OUTPUT_HIGHP(vec2 vHeightMapCoords)
VERTEX_OUTPUT_HIGHP(vec3 vDebug)
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

float FAST_32_hash( vec2 gridcell )
{
	//    gridcell is assumed to be an integer coordinate
	const vec2 OFFSET = vec2( 26.0, 161.0 );
	const float DOMAIN = 71.0;
	const float SOMELARGEFLOAT = 951.135664;
	vec4 P = vec4( gridcell.xy, gridcell.xy + 1.0.xx );
	P = P - floor(P * ( 1.0 / DOMAIN )) * DOMAIN;    //    truncate the domain
	P += OFFSET.xyxy;                                //    offset to interesting part of the noise
	P *= P;                                          //    calculate and return the hash
	return fract( P.xzxz * P.yyww * ( 1.0 / SOMELARGEFLOAT.x ).xxxx ).x;
}

void main()
{
    VertexTransform vertexTransform = GetVertexTransform();
	
	mat4 modelMatrix = GetModelMatrix();
	vec2 cell=floor(vertexTransform.position.xz);
	float hash=FAST_32_hash(cell);
	mat4 rot=rotationMatrix(vec3(0,1,0), hash*3.14);
	vertexTransform.position = iPos * rot * modelMatrix;
 
	//vec2 eyevec=vertexTransform.position.xz-cCameraPos.xz;
	//float dist=eyevec.length();
	//dist=(dist-cRadius.y)/(cRadius.x-cRadius.y);
	//dist=clamp(dist,0.0,1.0);
	
	float dx=vertexTransform.position.x - cCameraPos.x;
	float dz=vertexTransform.position.z - cCameraPos.z;
	float dist=sqrt(dx*dx+dz*dz);
	dist=(dist-cRadius.y)/(cRadius.x-cRadius.y);
	dist=clamp(dist,0.0,1.0);
	
	vec2 t=vertexTransform.position.xz / cHeightMapData.z;
	vec2 htuv=vec2((t.x/cHeightMapData.x)+0.5, 1.0-((t.y/cHeightMapData.y)+0.5));
	vec4 htt=textureLod(sHeightMap2, htuv, 0.0);
	float htscale=cHeightMapData.w*255.0;
	float ht=htt.r*htscale + htt.g*cHeightMapData.w;
	
	float covscale=smoothstep(0.5, 0.8, textureLod(sCoverageMap3, htuv, 0.0).b);
	float y=(vertexTransform.position.y)*dist*covscale*2 + ht - 0.25;

	vertexTransform.position.y=y;
	vScaleValue=dist*covscale;
	
	vDebug=cCameraPos.xyz-vertexTransform.position.xyz;
	
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
	
	//surfaceData.albedo=vec4(vDebug, 1);
	
	if(vScaleValue<=0.00001) discard;
	if(surfaceData.albedo.a<0.5) discard;
	
    half3 finalColor = GetFinalColor(surfaceData);
    gl_FragColor.rgb = ApplyFog(finalColor, surfaceData.fogFactor);
    gl_FragColor.a = GetFinalAlpha(surfaceData);
#endif
}
#endif