#pragma once
#include <d3d11.h>
#include <core/render/preview/lighting.h>

// Class to hold information related to the general render scene.
// Not directly related to any individual type of preview/render setup.
class CDXScene
{
public:
	CDXScene() = default;

	// Global Lights are used in Pixel Shaders as a StructuredBuffer<HardwareLight> resource in register t62.
	// The data must be bound using a shader resource view.
	ID3D11Buffer* globalLightsBuffer;
	ID3D11ShaderResourceView* globalLightsSRV;

	ID3D11Buffer* cubemapSamplesBuffer;
	ID3D11ShaderResourceView* cubemapSamplesSRV;

	std::vector<HardwareLight> globalLights;
	std::vector<float> cubemapSamples;

	// Update the HardwareLight structures in the globalLights vector
	void UpdateHardwareLights();
	void UpdateCubemapSamples();

	// Map and update the globalLights d3d buffer
	void MapAndUpdateLightBuffer(ID3D11Device* device, ID3D11DeviceContext* ctx);
	void MapAndUpdateCubemapSamplesBuffer(ID3D11Device* device, ID3D11DeviceContext* ctx);

	FORCEINLINE bool NeedsLightingUpdate() const
	{
		return this->invalidatedLighting || this->ShouldRecreateLightBuffer();
	}

	FORCEINLINE bool NeedsCubemapSmpUpdate() const
	{
		return this->ShouldRecreateCubemapSmpBuffer();
	}

private:

	// Forces the light buffer to be recreated next frame.
	// This is used when a light is modified but the number of lights doesn't change, as this would
	// usually not result in recreation of the buffer.
	FORCEINLINE void InvalidateLights() { invalidatedLighting = true; };
	FORCEINLINE bool ShouldRecreateLightBuffer() const { return !this->initialLightSetupComplete || this->numLightsInBuffer != globalLights.size(); };
	
	FORCEINLINE bool ShouldRecreateCubemapSmpBuffer() const { return !this->initialCubemapSmpSetupComplete || this->numCubemapSamples != cubemapSamples.size(); };

	// Number of lights in the current version of the Global Lights Buffer
	// This is stored separately to just using globalLights.size(), since the globalLights vector
	// may be updated between frames, and we should only have to recreate the buffer whenever there are changes to the size.
	size_t numLightsInBuffer;
	size_t numCubemapSamples;

	// If lighting data needs to be updated without the vector being resized.
	bool invalidatedLighting : 1;
	// If the initial version of the lighting buffer has been created.
	bool initialLightSetupComplete : 1;
	bool initialCubemapSmpSetupComplete : 1;
};