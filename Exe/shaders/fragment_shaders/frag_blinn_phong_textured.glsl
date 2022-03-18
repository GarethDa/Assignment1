#version 430

#include "../fragments/fs_common_inputs.glsl"

// We output a single color to the color buffer
layout(location = 0) out vec4 frag_color;

////////////////////////////////////////////////////////////////
/////////////// Instance Level Uniforms ////////////////////////
////////////////////////////////////////////////////////////////

// Represents a collection of attributes that would define a material
// For instance, you can think of this like material settings in 
// Unity
struct Material {
	sampler2D Diffuse;
	float     Shininess;
};
// Create a uniform for the material
uniform Material u_Material;
uniform sampler1D s_1Dtex;

////////////////////////////////////////////////////////////////
///////////// Application Level Uniforms ///////////////////////
////////////////////////////////////////////////////////////////

#include "../fragments/multiple_point_lights.glsl"

////////////////////////////////////////////////////////////////
/////////////// Frame Level Uniforms ///////////////////////////
////////////////////////////////////////////////////////////////

#include "../fragments/frame_uniforms.glsl"
#include "../fragments/color_correction.glsl"

/*
// https://learnopengl.com/Advanced-Lighting/Advanced-Lighting
void main() {
	// Normalize our input normal
	vec3 normal = normalize(inNormal);

	// Use the lighting calculation that we included from our partial file
	vec3 lightAccumulation = CalcAllLightContribution(inWorldPos, normal, u_CamPos.xyz, u_Material.Shininess);

	// Get the albedo from the diffuse / albedo map
	vec4 textureColor = texture(u_Material.Diffuse, inUV);

	// combine for the final result
	vec3 result = lightAccumulation  * inColor * textureColor.rgb;
	
	if (IsFlagSet(FLAG_DISABLE_LIGHTING)) 
	{
		frag_color = textureColor;
	}

	else 
	{
		frag_color = vec4(ColorCorrect(result), textureColor.a);
	}
	
}
*/


void main() {
	// Normalize our input normal
	vec3 normal = normalize(inNormal);

	// Get the albedo from the diffuse / albedo map
	vec4 textureColor = texture(u_Material.Diffuse, inUV);
	
	if (IsFlagSet(FLAG_DISABLE_LIGHTING)) 
	{
		frag_color = vec4(ColorCorrect(textureColor.rgb), textureColor.a);
	}

	else if (IsFlagSet(FLAG_ENABLE_AMBIENT))
	{
		vec3 lightAccumulation = CalcAmbient();
		vec3 result = lightAccumulation * inColor * textureColor.rgb;
		frag_color = vec4(result, textureColor.a);
	}

	else if (IsFlagSet(FLAG_ENABLE_SPECULAR))
	{
		vec3 lightAccumulation = CalcSpec(inWorldPos, normal, u_CamPos.xyz, u_Material.Shininess);
		vec3 result = lightAccumulation * inColor * textureColor.rgb;
		frag_color = vec4(ColorCorrect(result), textureColor.a);
	}

	else if (IsFlagSet(FLAG_ENABLE_AMBSPEC))
	{
		vec3 lightAccumulation = CalcSpec(inWorldPos, normal, u_CamPos.xyz, u_Material.Shininess) + CalcAmbient();
		vec3 result = lightAccumulation * inColor * textureColor.rgb;
		frag_color = vec4(ColorCorrect(result), textureColor.a);
	}
	
	else if (IsFlagSet(FLAG_ENABLE_CUSTOMSHADER))
	{
		//Emboss shader concept from https://www.raywenderlich.com/2941-how-to-create-cool-effects-with-custom-shaders-in-opengl-es-2-0-and-cocos2d-2-x
		
		//Size of one texel
		vec2 onePixel = vec2(1.0 / 1200.0, 1.0 / 800.0);

		//Create a temporary colour vector, make it grey
		vec4 tempColour;
		tempColour.rgb = vec3(0.5);

		//Add colour from both sides of the texel. This means that any time a drastic change in colour occurs, it's emphasized by the emboss effect.
		//Multiply by a scalar to get a more intense effect
		tempColour -= texture2D(u_Material.Diffuse, inUV - onePixel) * 8.0;
		tempColour += texture2D(u_Material.Diffuse, inUV + onePixel) * 8.0;
		
		//Average the colours to keep them grey
		tempColour.rgb = vec3((tempColour.r + tempColour.g + tempColour.b) / 3.0);

		//Multiply by lightAccumulation to keep the lighting in
		vec3 lightAccumulation = CalcAllLightContribution(inWorldPos, normal, u_CamPos.xyz, u_Material.Shininess);
		frag_color = vec4(tempColour.rgb * lightAccumulation, 1);
	}

	else if (IsFlagSet(FLAG_ENABLE_DIFFUSEWARP))
	{
		// combine for the final result
		vec3 result;

		// Using LUT
		result.r = texture(s_1Dtex, textureColor.r).r;
		result.g = texture(s_1Dtex, textureColor.g).g;
		result.b = texture(s_1Dtex, textureColor.b).b;

		frag_color = vec4(result, textureColor.a);
		}

	else if (IsFlagSet(FLAG_ENABLE_SPECWARP))
	{
		// Use the lighting calculation that we included from our partial file
		vec3 lightAccumulation = CalcSpec(inWorldPos, normal, u_CamPos.xyz, u_Material.Shininess);

		// combine for the final result
		vec3 result;

		// Using a LUT to allow artists to tweak toon shading settings
		result.r = texture(s_1Dtex, lightAccumulation.r).r;
		result.g = texture(s_1Dtex, lightAccumulation.g).g;
		result.b = texture(s_1Dtex, lightAccumulation.b).b;

		result = result  * inColor * textureColor.rgb;

		frag_color = vec4(result, textureColor.a);
	}

	else
	{
		// Use the lighting calculation that we included from our partial file
		vec3 lightAccumulation = CalcAllLightContribution(inWorldPos, normal, u_CamPos.xyz, u_Material.Shininess);

		// Get the albedo from the diffuse / albedo map
		vec4 textureColor = texture(u_Material.Diffuse, inUV);

		// combine for the final result
		vec3 result = lightAccumulation  * inColor * textureColor.rgb;

		frag_color = vec4(ColorCorrect(result), textureColor.a);
	}

}