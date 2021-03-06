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

uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;

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

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;


vec3 calculateDirectIllumiunation(vec3 wo, vec3 n, vec3 base_color)
{
	//vec3 direct_illum = base_color;
	///////////////////////////////////////////////////////////////////////////
	// Task 1.2 - Calculate the radiance Li from the light, and the direction
	//            to the light. If the light is backfacing the triangle,
	//            return vec3(0);
	///////////////////////////////////////////////////////////////////////////

	float dist = distance(viewSpacePosition, viewSpaceLightPosition);
	vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);
	vec3 wh = normalize(wi + wo);

	if (dot(n, wi) <= 0.0f)
	{
		return vec3(0.0f);
	}

	vec3 Li = point_light_intensity_multiplier * point_light_color * 1/pow(dist, 2);

		///////////////////////////////////////////////////////////////////////////
		// Task 1.3 - Calculate the diffuse term and return that as the result
		///////////////////////////////////////////////////////////////////////////
		// vec3 diffuse_term = ...

	vec3 diffuse_term = material_color * (1.0f/PI) * abs(dot(n, wi)) * Li;

	///////////////////////////////////////////////////////////////////////////
	// Task 2 - Calculate the Torrance Sparrow BRDF and return the light
	//          reflected from that instead
	///////////////////////////////////////////////////////////////////////////

	float D_wh = ((material_shininess + 2.0f)/(2.0f * PI)) * pow(max(0.00001f, dot(n, wh)), material_shininess);

	float G_wi_wo = min(1.0f, min(2.0f * dot(n, wh) * dot(n, wo) / dot(wo, wh), 2.0f * dot(n, wh) * dot(n, wi) / dot(wo, wh)));

	float F_wi = material_fresnel + (1.0f - material_fresnel) * pow(max(0.00001f, 1.0f - dot(wh, wi)), 5);

	float brdf = (F_wi * D_wh * G_wi_wo) / (4 * dot(n, wo) * dot(n, wi));

	//////////////////////////////////////////////////////////////////////////
	// Task 3 - Make your shader respect the parameters of our material model.
	//////////////////////////////////////////////////////////////////////////

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

vec3 calculateIndirectIllumination(vec3 wo, vec3 n, vec3 base_color)
{
	///////////////////////////////////////////////////////////////////////////
	// Task 5 - Lookup the irradiance from the irradiance map and calculate
	//          the diffuse reflection
	///////////////////////////////////////////////////////////////////////////

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

	///////////////////////////////////////////////////////////////////////////
	// Task 6 - Look up in the reflection map from the perfect specular
	//          direction and calculate the dielectric and metal terms.
	///////////////////////////////////////////////////////////////////////////

	//vec3 wi = normalize(reflect(wo, n));
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
	///////////////////////////////////////////////////////////////////////////
	// Task 1.1 - Fill in the outgoing direction, wo, and the normal, n. Both
	//            shall be normalized vectors in view-space.
	///////////////////////////////////////////////////////////////////////////
	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	vec3 base_color = material_color;
	if(has_color_texture == 1)
	{
		base_color *= texture(colorMap, texCoord).xyz;
	}

	vec3 direct_illumination_term = vec3(0.0);
	{ // Direct illumination
		direct_illumination_term = calculateDirectIllumiunation(wo, n, base_color);
	}

	vec3 indirect_illumination_term = vec3(0.0);
	{ // Indirect illumination
		indirect_illumination_term = calculateIndirectIllumination(wo, n, base_color);
	}

	///////////////////////////////////////////////////////////////////////////
	// Task 1.4 - Make glowy things glow!
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;

	vec3 final_color = direct_illumination_term + indirect_illumination_term + emission_term;

	// Check if we got invalid results in the operations
	if(any(isnan(final_color)))
	{
		final_color.xyz = vec3(1.f, 0.f, 1.f);
	}

	fragmentColor.xyz = final_color;
}
