#define URHO3D_CUSTOM_MATERIAL_UNIFORMS
#define URHO3D_DISABLE_DIFFUSE_SAMPLING
#define URHO3D_DISABLE_NORMAL_SAMPLING
#define URHO3D_DISABLE_SPECULAR_SAMPLING
#define URHO3D_DISABLE_EMISSIVE_SAMPLING

#ifdef NORMALMAP
	#define URHO3D_VERTEX_NEED_TANGENT
#endif

#define URHO3D_PIXEL_NEED_VERTEX_COLOR

#include "_Config.glsl"
#include "_Uniforms.glsl"

UNIFORM_BUFFER_BEGIN(4, Material)
    DEFAULT_MATERIAL_UNIFORMS
    UNIFORM(vec3 cDetailTiling)
	UNIFORM(vec4 cLayerScaling)
	#ifdef SCATTERING
		UNIFORM(float cTimeOfDay)
		UNIFORM(float cBr)
		UNIFORM(float cBm)
	#endif
UNIFORM_BUFFER_END(4, Material)

#include "_Material.glsl"

VERTEX_OUTPUT_HIGHP(vec3 vDetailTexCoord)
VERTEX_OUTPUT_HIGHP(vec2 vBlendTexCoord)

#ifdef SCATTERING
	VERTEX_OUTPUT_HIGHP(vec3 vSun)
	VERTEX_OUTPUT_HIGHP(vec3 vFogPos)
#endif

uniform sampler2DArray sDetailMap0;
#ifdef NORMALMAP
	uniform sampler2DArray sNormal1;
#endif
uniform sampler2D sWeightMap2;

#ifdef URHO3D_VERTEX_SHADER
void main()
{
    VertexTransform vertexTransform = GetVertexTransform();
    FillVertexOutputs(vertexTransform);
    vDetailTexCoord = vertexTransform.position.xyz * cDetailTiling;
	vBlendTexCoord = (vertexTransform.position.xz / 1024.0) * 0.5 + 0.5;
	vBlendTexCoord.y = 1.0 - vBlendTexCoord.y;
	#ifdef SCATTERING
		 mat4 modelMatrix = GetModelMatrix();
		 vec4 wPos = vec4(iPos.xyz, 0.0) * modelMatrix;
		vSun = vec3(cos(cTimeOfDay * 0.2617993875), sin(cTimeOfDay * 0.2617993875), 0);
		vFogPos = vertexTransform.position.xyz;
		//vFogPos = normalize(wPos.xyz - cCameraPos.xyz)
	#endif
}
#endif

#ifdef URHO3D_PIXEL_SHADER
	#ifdef SCATTERING
		vec4 CalculateScattering(float Br, float Bm, float g, vec3 pos, vec3 fsun, inout vec3 extinction)
		{
			const vec3 nitrogen = vec3(0.650, 0.570, 0.475);
			vec3 Kr = Br / pow(nitrogen, vec3(4.0));
			vec3 Km = Bm / pow(nitrogen, vec3(0.84));
			vec4 color=vec4(0,0,0,1);
	
			//if (pos.y < 0) pos.y=0;
			pos.y = max(0, pos.y);
			// Atmosphere Scattering
			float mu = dot(normalize(pos), normalize(fsun));
			float rayleigh = 3.0 / (8.0 * 3.14) * (1.0 + mu * mu);
			vec3 mie = (Kr + Km * (1.0 - g * g) / (2.0 + g * g) / pow(1.0 + g * g - 2.0 * g * mu, 1.5)) / (Br + Bm);

			vec3 day_extinction = exp(-exp(-((pos.y + fsun.y * 4.0) * (exp(-pos.y * 16.0) + 0.1) / 80.0) / Br) * (exp(-pos.y * 16.0) + 0.1) * Kr / Br) * exp(-pos.y * exp(-pos.y * 8.0 ) * 4.0) * exp(-pos.y * 2.0) * 4.0;
			vec3 night_extinction = vec3(1.0 - exp(fsun.y)) * 0.2;
			extinction = mix(day_extinction, night_extinction, -fsun.y * 0.2 + 0.5);
			color.rgb = rayleigh * mie * extinction;
		
			return color;
		}  
	#endif

	vec4 BalanceColors(vec4 col)
	{
		float rem=1.0;
		vec4 o;
		o.r = min(rem, col.r);
		rem=max(0, rem-o.r);
		o.g = min(rem, col.g);
		rem=max(0, rem-o.g);
		o.b = min(rem, col.b);
		rem=max(0, rem-o.b);
		o.a = rem;
		return o;
	}

#ifndef REDUCETILING
	vec4 SampleDiffuse(vec3 detailtexcoord, int layer, float layerscaling, vec3 blend)
	{
		return texture(sDetailMap0, vec3(detailtexcoord.zy*layerscaling, layer))*blend.x +
			texture(sDetailMap0, vec3(detailtexcoord.xy*layerscaling, layer))*blend.z +
			texture(sDetailMap0, vec3(detailtexcoord.xz*layerscaling, layer))*blend.y;
	}

	#ifdef NORMALMAP
		vec3 SampleNormal(vec3 detailtexcoord, int layer, float layerscaling, vec3 blend)
		{
		return DecodeNormal(texture(sNormal1, vec3(detailtexcoord.zy*layerscaling, layer)))*blend.x+
			DecodeNormal(texture(sNormal1, vec3(detailtexcoord.xy*layerscaling, layer)))*blend.z+
			DecodeNormal(texture(sNormal1, vec3(detailtexcoord.xz*layerscaling,layer)))*blend.y;
	}
	#endif
#else
	vec4 SampleDiffuse(vec3 detailtexcoord, int layer, float layerscaling, vec3 blend)
	{
		return (texture(sDetailMap0, vec3(detailtexcoord.zy*layerscaling, layer))+texture(sDetailMap0, vec3(detailtexcoord.zy*layerscaling*0.27, layer)))*blend.x*0.5 +
			(texture(sDetailMap0, vec3(detailtexcoord.xy*layerscaling, layer))+texture(sDetailMap0, vec3(detailtexcoord.xy*layerscaling*0.27, layer)))*blend.z*0.5 +
			(texture(sDetailMap0, vec3(detailtexcoord.xz*layerscaling, layer))+texture(sDetailMap0, vec3(detailtexcoord.xz*layerscaling*0.27, layer)))*blend.y*0.5;
	}

	#ifdef NORMALMAP
	vec3 SampleNormal(vec3 detailtexcoord, int layer, float layerscaling, vec3 blend)
	{
		return (DecodeNormal(texture(sNormal1, vec3(detailtexcoord.zy*layerscaling, layer)))+DecodeNormal(texture(sNormal1, vec3(detailtexcoord.zy*layerscaling*0.27, layer))))*blend.x*0.5+
			(DecodeNormal(texture(sNormal1, vec3(detailtexcoord.xy*layerscaling, layer)))+DecodeNormal(texture(sNormal1, vec3(detailtexcoord.xy*layerscaling*0.27, layer))))*blend.z*0.5+
			(DecodeNormal(texture(sNormal1, vec3(detailtexcoord.xz*layerscaling,layer)))+DecodeNormal(texture(sNormal1, vec3(detailtexcoord.xz*layerscaling*0.27,layer))))*blend.y*0.5;
	}
	#endif
#endif


void main()
{
    SurfaceData surfaceData;

    FillSurfaceCommon(surfaceData);
    FillSurfaceNormal(surfaceData);
    FillSurfaceMetallicRoughnessOcclusion(surfaceData);
    FillSurfaceReflectionColor(surfaceData);
    FillSurfaceBackground(surfaceData);
	
	vec4 weights0 = BalanceColors(vColor);
	vec4 weights2 = texture2D(sWeightMap2, vBlendTexCoord);
	
	vec3 nrm = normalize(vNormal);
	vec3 blending=abs(nrm);
	blending = normalize(max(blending, 0.00001));
	float b=blending.x+blending.y+blending.z;
	blending=blending/b;

	vec4 tex1=SampleDiffuse(vDetailTexCoord, 0, cLayerScaling.r, blending);
	vec4 tex2=SampleDiffuse(vDetailTexCoord, 1, cLayerScaling.g, blending);
	vec4 tex3=SampleDiffuse(vDetailTexCoord, 2, cLayerScaling.b, blending);
	vec4 tex4=SampleDiffuse(vDetailTexCoord, 3, cLayerScaling.a, blending);
	
	weights0 = mix(weights0, weights2, smoothstep(0.0,1.0, blending.y));
	
	#ifndef SMOOTHBLEND
		float ma=max(tex1.a+weights0.r, max(tex2.a+weights0.g, max(tex3.a+weights0.b, tex4.a+weights0.a)))-0.2;
		float b1=max(0, tex1.a+weights0.r-ma);
		float b2=max(0, tex2.a+weights0.g-ma);
		float b3=max(0, tex3.a+weights0.b-ma);
		float b4=max(0, tex4.a+weights0.a-ma);
	#else
		float b1=weights0.r;
		float b2=weights0.g;
		float b3=weights0.b;
		float b4=weights0.a;
	#endif
	
	float bsum=b1+b2+b3+b4;
	vec4 diffColor=(tex1*b1+tex2*b2+tex3*b3+tex4*b4)/bsum;
	diffColor.a=1.0;
	
	#ifdef NORMALMAP
        mediump mat3 tbn = mat3(vTangent.xyz, vec3(vBitangentXY.xy, vTangent.w), vNormal);
		vec3 bump1=SampleNormal(vDetailTexCoord, 0, cLayerScaling.r, blending);
		vec3 bump2=SampleNormal(vDetailTexCoord, 1, cLayerScaling.g, blending);
		vec3 bump3=SampleNormal(vDetailTexCoord, 2, cLayerScaling.b, blending);
		vec3 bump4=SampleNormal(vDetailTexCoord, 3, cLayerScaling.a, blending);
	
		vec3 normal=tbn*normalize(((bump1*b1+bump2*b2+bump3*b3+bump4*b4)/bsum));

    #else
        vec3 normal = normalize(vNormal);
    #endif
	
	surfaceData.albedo = GammaToLightSpaceAlpha(cMatDiffColor) * GammaToLightSpaceAlpha(diffColor);
	surfaceData.normal = normal;
	
	#ifdef URHO3D_SURFACE_NEED_AMBIENT
		surfaceData.emission = GammaToLightSpace(cMatEmissiveColor);
	#endif
	
	half3 finalColor = GetFinalColor(surfaceData);
	//gl_FragColor.rgb = ApplyFog(finalColor, surfaceData.fogFactor);
	//gl_FragColor.a = GetFinalAlpha(surfaceData);
	#ifdef SCATTERING
		vec3 extinction;
		vec3 fogpos = normalize(vFogPos.xyz - cCameraPos.xyz);
		vec4 fogcolor=CalculateScattering(cBr, cBm, 1.0, fogpos, vSun, extinction);
		#ifndef URHO3D_ADDITIVE_LIGHT_PASS
			gl_FragColor.rgb = mix(fogcolor.rgb, finalColor, surfaceData.fogFactor);
		#else
			gl_FragColor.rgb = finalColor * surfaceData.fogFactor;
		#endif
	#else
			gl_FragColor.rgb = ApplyFog(finalColor, surfaceData.fogFactor);
	#endif
	
	//gl_FragColor.rgb = ApplyFog(finalColor, surfaceData.fogFactor);
	gl_FragColor.a = GetFinalAlpha(surfaceData);
}
#endif
