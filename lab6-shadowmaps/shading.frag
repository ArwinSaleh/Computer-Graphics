#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform int has_emission_texture;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

// Task 5
uniform vec3 viewSpaceLightDir;
uniform float spotOuterAngle;
uniform float spotInnerAngle;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;

// Task 2
in vec4 shadowMapCoord;
//layout(binding = 10) uniform sampler2D shadowMapTex;

// Task 7
layout(binding = 10) uniform sampler2DShadow shadowMapTex;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;



vec3 calculateDirectIllumiunation(vec3 wo, vec3 n)
{
	float dist = distance(viewSpacePosition, viewSpaceLightPosition);
	vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);
	vec3 wh = normalize(wi + wo);

	if (dot(n, wi) <= 0.0f)
	{
		return vec3(0.0f);
	}

	vec3 Li = point_light_intensity_multiplier * point_light_color * 1/pow(dist, 2);

	vec3 diffuse_term = material_color * (1.0f/PI) * abs(dot(n, wi)) * Li;

	float D_wh = ((material_shininess + 2.0f)/(2.0f * PI)) * pow(max(0.00001f, dot(n, wh)), material_shininess);

	float G_wi_wo = min(1.0f, min(2.0f * dot(n, wh) * dot(n, wo) / dot(wo, wh), 2.0f * dot(n, wh) * dot(n, wi) / dot(wo, wh)));

	float F_wi = material_fresnel + (1.0f - material_fresnel) * pow(max(0.00001f, 1.0f - dot(wh, wi)), 5);

	float brdf = (F_wi * D_wh * G_wi_wo) / (4 * dot(n, wo) * dot(n, wi));

	vec3 dielectric_term = brdf * dot(n, wi) * Li + (1 - F_wi) * diffuse_term;

	vec3 metal_term = brdf * material_color * dot(n, wi) * Li;
	
	vec3 microfacet_term = material_metalness * metal_term + (1 - material_metalness) * dielectric_term;

	//return base_color;
	//return diffuse_term;
	//return vec3(D_wh);
	//return vec3(G_wi_wo);
	//return vec3(F_wi);
	//return brdf * dot(n, wi) * Li; 
	return material_reflectivity * microfacet_term + (1 - material_reflectivity) * diffuse_term;
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	// Calculate the world-space direction from the camera to that position
	vec4 dir = normalize(viewInverse * vec4(n, 0.0f));

	// Calculate the spherical coordinates of the direction
	float theta = acos(max(-1.0f, min(1.0f, dir.y)));
	float phi = atan(dir.z, dir.x);
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}

	// Use these to lookup the color in the environment map
	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
	vec3 indirect_illum = environment_multiplier * texture(irradianceMap, lookup).xyz;

	vec3 diffuse_term = material_color * (1.0f / PI) * indirect_illum;

	vec3 wi = reflect(-wo, n);
	vec3 wh = normalize(wi + wo);
	vec4 wiWorldSpace = viewInverse * vec4(wi, 0.0f);
	float roughness = sqrt(sqrt(2.0f / (material_shininess + 2.0f)));

	theta = acos(max(-1.0f, min(1.0f, wiWorldSpace.y)));
	phi = atan(wiWorldSpace.z, wiWorldSpace.x);
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}
	lookup = vec2(phi / (2.0 * PI), theta / PI);

	indirect_illum = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0f).xyz;

	float F_wi = material_fresnel + (1.0f - material_fresnel) * pow(max(0.00001f, 1.0f - dot(wo, wh)), 5);
	vec3 dielectric_term = F_wi * indirect_illum + (1 - F_wi) * diffuse_term;
	vec3 metal_term = F_wi * material_color * indirect_illum;
	vec3 microfacet_term = material_metalness * metal_term + (1.0f - material_metalness) * dielectric_term;

	//return indirect_illum;
	//return diffuse_term;
	return material_reflectivity * microfacet_term + (1.0f - material_reflectivity) * diffuse_term;
}

void main()
{
	//float visibility = 1.0;
	float attenuation = 1.0;

	// Task 2
	//float depth= texture( shadowMapTex, shadowMapCoord.xy/shadowMapCoord.w ).r;
	//float visibility= (depth>=(shadowMapCoord.z/shadowMapCoord.w)) ? 1.0 : 0.0;
	
	// Task 7
	/*
	textureProj performs a texture lookup with projection. The texture coordinates consumed
    from P, not including the last component of P, are divided by the last component of P. The
    resulting 3rd component of P in the shadow forms is used as Dref. After these values are
    computed, the texture lookup proceeds as in texture().

	Basically textureProj() does the same as texture() but with projection, using sampler2DShadow
	for shadow mapping when reading depth textures.
	*/
	float visibility = textureProj( shadowMapTex, shadowMapCoord );

	// Task 5
	vec3 posToLight = normalize(viewSpaceLightPosition - viewSpacePosition);
	float cosAngle = dot(posToLight, -viewSpaceLightDir);
	// Spotlight with hard border:
	//float spotAttenuation = (cosAngle > spotOuterAngle) ? 1.0 : 0.0;
	float spotAttenuation = smoothstep(spotOuterAngle, spotInnerAngle, cosAngle);
	visibility *= spotAttenuation;

	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	// Direct illumination
	vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;
	if(has_emission_texture == 1)
	{
		emission_term = texture(emissiveMap, texCoord).xyz;
	}

	vec3 shading = direct_illumination_term + indirect_illumination_term + emission_term;

	fragmentColor = vec4(shading, 1.0);
	return;
}
