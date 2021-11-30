#include "material.h"
#include "sampling.h"

namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////
// A Lambertian (diffuse) material
///////////////////////////////////////////////////////////////////////////
vec3 Diffuse::f(const vec3& wi, const vec3& wo, const vec3& n)
{
	if(dot(wi, n) <= 0.0f)
		return vec3(0.0f);
	if(!sameHemisphere(wi, wo, n))
		return vec3(0.0f);
	return (1.0f / M_PI) * color;
}

vec3 Diffuse::sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p)
{
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	vec3 sample = cosineSampleHemisphere();
	wi = normalize(sample.x * tangent + sample.y * bitangent + sample.z * n);
	if(dot(wi, n) <= 0.0f)
		p = 0.0f;
	else
		p = max(0.0f, dot(n, wi)) / M_PI;
	return f(wi, wo, n);
}

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Dielectric Microfacet BRFD
///////////////////////////////////////////////////////////////////////////
vec3 BlinnPhong::refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	// Task 3: Blinn Phong BRDF

	if (refraction_layer == NULL)
	{
		return vec3(0.0f);
	}
	vec3 wh = normalize(wi + wo);
	float wh_wi = max(0.0f, dot(wh, wi));
	float F_wi = R0 + (1.0f - R0) * pow(1.0f - wh_wi, 5.0f);


	return (1 - F_wi) * refraction_layer->f(wi, wo, n);
}
vec3 BlinnPhong::reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	// Task 3: Blinn Phong BRDF

	if (dot(n, wi) <= 0.0f)
	{
		return vec3(0.0f);
	}

	vec3 wh = normalize(wi + wo);

	// Computing D(wh), G(wi, wo) and F(wi).
	float D_wh = ((shininess + 2.0f) / (2.0f * M_PI)) * pow(max(0.00001f, dot(n, wh)), shininess);
	float G_wi_wo = min(1.0f, min(2.0f * dot(n, wh) * dot(n, wo) / dot(wo, wh), 2.0f * dot(n, wh) * dot(n, wi) / dot(wo, wh)));
	float F_wi = R0 + (1.0f - R0) * pow(max(0.00001f, 1.0f - dot(wh, wi)), 5);

	float brdf = (F_wi * D_wh * G_wi_wo) / (4 * dot(n, wo) * dot(n, wi));

	return vec3(brdf);
}

vec3 BlinnPhong::f(const vec3& wi, const vec3& wo, const vec3& n)
{
	return reflection_brdf(wi, wo, n) + refraction_brdf(wi, wo, n);
}

vec3 BlinnPhong::sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p)
{
	
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	float phi = 2.0f * M_PI * randf();
	float cos_theta = pow(randf(), 1.0f / (shininess + 1));
	float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
	vec3 wh = normalize(sin_theta * cos(phi) * tangent +
		sin_theta * sin(phi) * bitangent +
		cos_theta * n);
	if (dot(wo, n) <= 0.0f) return vec3(0.0f);
	

	// Task 6
	if (randf() < 0.5f)
	{
		// Sample a direction based on the Microfacet brdf
		wi = reflect(-wo, wh);
		float p_wh = (shininess + 1.0f) * pow(dot(n, wh), shininess) / (2.0f * M_PI);
		float p_wi = p_wh / (4.0f * dot(wo, wh));
		p = p_wi;
		p *= 0.5f;

		return reflection_brdf(wi, wo, n);
	}
	else
	{
		// Sample a direction for the underlying layer
		vec3 brdf = refraction_layer->sample_wi(wi, wo, n, p);
		p *= 0.5f;

		// We need to attenuate the refracted brdf with (1 - F)
		float F = R0 + (1.0f - R0) * pow(1.0f - abs(dot(wh, wi)), 5.0f);

		return (1.0f - F) * brdf;

	}
}

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Metal Microfacet BRFD (extends the BlinnPhong class)
///////////////////////////////////////////////////////////////////////////
vec3 BlinnPhongMetal::refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	return vec3(0.0f);
}
vec3 BlinnPhongMetal::reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	return BlinnPhong::reflection_brdf(wi, wo, n) * color;
};

///////////////////////////////////////////////////////////////////////////
// A Linear Blend between two BRDFs
///////////////////////////////////////////////////////////////////////////
vec3 LinearBlend::f(const vec3& wi, const vec3& wo, const vec3& n)
{
	// Task 4
	// 'w' between 0 and 1
	return w * bsdf0->f(wi, wo, n) + (1 - w) * bsdf1->f(wi, wo, n);
}

vec3 LinearBlend::sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p)
{
	p = 0.0f;	// Probability
	return vec3(0.0f);
}

///////////////////////////////////////////////////////////////////////////
// A perfect specular refraction.
///////////////////////////////////////////////////////////////////////////
} // namespace pathtracer