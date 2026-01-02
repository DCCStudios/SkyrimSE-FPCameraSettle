#include "Graphics.h"
#include "Settings.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace Graphics
{
	namespace
	{
		// Fullscreen vertex shader - generates a fullscreen triangle
		const char* fullscreenVSSource = R"(
struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT main(uint VertexID : SV_VertexID)
{
    VS_OUTPUT output;
    
    // Generate fullscreen triangle vertices
    // VertexID 0: (-1, -1) -> (0, 1)
    // VertexID 1: (-1,  3) -> (0, -1)
    // VertexID 2: ( 3, -1) -> (2, 1)
    output.TexCoord = float2((VertexID << 1) & 2, VertexID & 2);
    output.Position = float4(output.TexCoord.x * 2.0 - 1.0, -output.TexCoord.y * 2.0 + 1.0, 0.0, 1.0);
    
    return output;
}
)";

		// Radial blur pixel shader - cheap and effective
		const char* radialBlurPSSource = R"(
Texture2D screenTexture : register(t0);
SamplerState linearSampler : register(s0);

cbuffer BlurParams : register(b0)
{
    float strength;     // Blur intensity (0-1)
    float centerX;      // Blur center X (0.5 = screen center)
    float centerY;      // Blur center Y (0.5 = screen center)
    float sampleCount;  // Number of samples
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // Early out if no blur
    if (strength <= 0.001)
    {
        return screenTexture.Sample(linearSampler, input.TexCoord);
    }
    
    float2 center = float2(centerX, centerY);
    float2 dir = input.TexCoord - center;
    float dist = length(dir);
    
    // Skip blur near center
    if (dist < 0.01)
    {
        return screenTexture.Sample(linearSampler, input.TexCoord);
    }
    
    dir = normalize(dir);
    
    // Accumulate samples along the radial direction
    float4 color = float4(0, 0, 0, 0);
    int samples = (int)sampleCount;
    float blurAmount = strength * dist * 0.1; // Scale blur by distance from center
    
    for (int i = 0; i < samples; i++)
    {
        float t = (float)i / (float)(samples - 1);
        float offset = blurAmount * (t - 0.5) * 2.0;
        float2 sampleUV = input.TexCoord - dir * offset;
        
        // Clamp UV to prevent sampling outside texture
        sampleUV = clamp(sampleUV, 0.001, 0.999);
        
        color += screenTexture.Sample(linearSampler, sampleUV);
    }
    
    return color / (float)samples;
}
)";

		// Hook for IDXGISwapChain::Present
		using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
		PresentFn originalPresent = nullptr;
		
		HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
		{
			auto* manager = RadialBlurManager::GetSingleton();
			
			// Apply radial blur if strength > 0
			if (manager->IsInitialized()) {
				auto* settings = Settings::GetSingleton();
				if (settings->sprintBlurEnabled) {
					manager->ApplyRadialBlur();
				}
			}
			
			return originalPresent(swapChain, syncInterval, flags);
		}
		
		// Hook for IDXGISwapChain vtable
		struct PresentHook
		{
			static void thunk(std::uint32_t a_timer)
			{
				func(a_timer);
				
				static bool initialized = false;
				if (!initialized) {
					auto* manager = RadialBlurManager::GetSingleton();
					manager->Initialize();
					initialized = true;
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static inline constexpr std::size_t size{ 5 };
		};
	}

	void RadialBlurManager::Initialize()
	{
		if (initialized) {
			return;
		}
		
		// Get renderer and D3D11 device from Skyrim
		auto* rendererData = RE::BSGraphics::Renderer::GetRendererData();
		if (!rendererData) {
			logger::error("[Graphics] Failed to get BSGraphics::RendererData");
			return;
		}
		
		// Cast from REX::W32 types to Microsoft types
		auto* dxgiSwapChain = reinterpret_cast<IDXGISwapChain*>(rendererData->renderWindows[0].swapChain);
		if (!dxgiSwapChain) {
			logger::error("[Graphics] Failed to get swap chain");
			return;
		}
		swapChain = dxgiSwapChain;
		
		// Get device and context
		if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device)))) {
			logger::error("[Graphics] Failed to get D3D11 device from swap chain");
			return;
		}
		
		device->GetImmediateContext(&context);
		if (!context) {
			logger::error("[Graphics] Failed to get D3D11 device context");
			return;
		}
		
		// Create shaders and resources
		if (!CreateShaders()) {
			logger::error("[Graphics] Failed to create shaders");
			return;
		}
		
		if (!CreateResources()) {
			logger::error("[Graphics] Failed to create resources");
			return;
		}
		
		// Hook the Present function via vtable
		void** vtable = *reinterpret_cast<void***>(swapChain.Get());
		originalPresent = reinterpret_cast<PresentFn>(vtable[8]);  // Present is at index 8
		
		// Write hook using memory protection change
		DWORD oldProtect;
		VirtualProtect(&vtable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
		vtable[8] = reinterpret_cast<void*>(&HookedPresent);
		VirtualProtect(&vtable[8], sizeof(void*), oldProtect, &oldProtect);
		
		initialized = true;
		logger::info("[Graphics] Radial blur system initialized successfully");
	}

	void RadialBlurManager::Shutdown()
	{
		if (!initialized) {
			return;
		}
		
		ReleaseResources();
		initialized = false;
	}

	bool RadialBlurManager::CreateShaders()
	{
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		
		UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
		
		// Compile vertex shader
		HRESULT hr = D3DCompile(
			fullscreenVSSource,
			strlen(fullscreenVSSource),
			"FullscreenVS",
			nullptr,
			nullptr,
			"main",
			"vs_5_0",
			compileFlags,
			0,
			&vsBlob,
			&errorBlob
		);
		
		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("[Graphics] VS compile error: {}", static_cast<const char*>(errorBlob->GetBufferPointer()));
			}
			return false;
		}
		
		hr = device->CreateVertexShader(
			vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			nullptr,
			&fullscreenVS
		);
		
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to create vertex shader");
			return false;
		}
		
		// Compile pixel shader
		hr = D3DCompile(
			radialBlurPSSource,
			strlen(radialBlurPSSource),
			"RadialBlurPS",
			nullptr,
			nullptr,
			"main",
			"ps_5_0",
			compileFlags,
			0,
			&psBlob,
			&errorBlob
		);
		
		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("[Graphics] PS compile error: {}", static_cast<const char*>(errorBlob->GetBufferPointer()));
			}
			return false;
		}
		
		hr = device->CreatePixelShader(
			psBlob->GetBufferPointer(),
			psBlob->GetBufferSize(),
			nullptr,
			&radialBlurPS
		);
		
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to create pixel shader");
			return false;
		}
		
		logger::info("[Graphics] Shaders compiled successfully");
		return true;
	}

	bool RadialBlurManager::CreateResources()
	{
		// Create constant buffer
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = sizeof(BlurConstants);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		
		HRESULT hr = device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to create constant buffer");
			return false;
		}
		
		// Create sampler state
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		
		hr = device->CreateSamplerState(&samplerDesc, &linearSampler);
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to create sampler state");
			return false;
		}
		
		// Get back buffer dimensions and create copy texture
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to get back buffer");
			return false;
		}
		
		D3D11_TEXTURE2D_DESC bbDesc;
		backBuffer->GetDesc(&bbDesc);
		backBufferWidth = bbDesc.Width;
		backBufferHeight = bbDesc.Height;
		
		// Create copy texture for reading the back buffer
		D3D11_TEXTURE2D_DESC copyDesc = bbDesc;
		copyDesc.Usage = D3D11_USAGE_DEFAULT;
		copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		
		hr = device->CreateTexture2D(&copyDesc, nullptr, &backBufferCopy);
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to create back buffer copy texture");
			return false;
		}
		
		// Create SRV for the copy
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = bbDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		
		hr = device->CreateShaderResourceView(backBufferCopy.Get(), &srvDesc, &backBufferCopySRV);
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to create SRV for back buffer copy");
			return false;
		}
		
		// Create RTV for the back buffer
		hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferRTV);
		if (FAILED(hr)) {
			logger::error("[Graphics] Failed to create RTV for back buffer");
			return false;
		}
		
		logger::info("[Graphics] Resources created successfully ({}x{})", backBufferWidth, backBufferHeight);
		return true;
	}

	void RadialBlurManager::ReleaseResources()
	{
		backBufferRTV.Reset();
		backBufferCopySRV.Reset();
		backBufferCopy.Reset();
		linearSampler.Reset();
		constantBuffer.Reset();
		radialBlurPS.Reset();
		fullscreenVS.Reset();
	}

	void RadialBlurManager::SetBlurStrength(float a_strength)
	{
		blurParams.strength = std::clamp(a_strength, 0.0f, 1.0f);
	}

	void RadialBlurManager::SetBlurCenter(float a_x, float a_y)
	{
		blurParams.centerX = std::clamp(a_x, 0.0f, 1.0f);
		blurParams.centerY = std::clamp(a_y, 0.0f, 1.0f);
	}

	void RadialBlurManager::ApplyRadialBlur()
	{
		if (!initialized || blurParams.strength <= 0.001f) {
			return;
		}
		
		// Get current back buffer
		Microsoft::WRL::ComPtr<ID3D11Texture2D> currentBackBuffer;
		HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&currentBackBuffer));
		if (FAILED(hr)) {
			return;
		}
		
		// Check if we need to recreate resources (resolution change)
		D3D11_TEXTURE2D_DESC bbDesc;
		currentBackBuffer->GetDesc(&bbDesc);
		if (bbDesc.Width != backBufferWidth || bbDesc.Height != backBufferHeight) {
			// Resolution changed, recreate resources
			ReleaseResources();
			if (!CreateResources()) {
				return;
			}
		}
		
		// Copy back buffer to our texture
		context->CopyResource(backBufferCopy.Get(), currentBackBuffer.Get());
		
		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped;
		hr = context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (SUCCEEDED(hr)) {
			memcpy(mapped.pData, &blurParams, sizeof(BlurConstants));
			context->Unmap(constantBuffer.Get(), 0);
		}
		
		// Save current state
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTV;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
		context->OMGetRenderTargets(1, &oldRTV, &oldDSV);
		
		D3D11_VIEWPORT oldViewport;
		UINT numViewports = 1;
		context->RSGetViewports(&numViewports, &oldViewport);
		
		Microsoft::WRL::ComPtr<ID3D11VertexShader> oldVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> oldPS;
		context->VSGetShader(&oldVS, nullptr, nullptr);
		context->PSGetShader(&oldPS, nullptr, nullptr);
		
		// Create RTV for current back buffer (needed because the old one might be stale)
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentRTV;
		device->CreateRenderTargetView(currentBackBuffer.Get(), nullptr, &currentRTV);
		
		// Set up for fullscreen pass
		D3D11_VIEWPORT viewport = {};
		viewport.Width = static_cast<float>(backBufferWidth);
		viewport.Height = static_cast<float>(backBufferHeight);
		viewport.MaxDepth = 1.0f;
		
		context->RSSetViewports(1, &viewport);
		context->OMSetRenderTargets(1, currentRTV.GetAddressOf(), nullptr);
		
		// Set shaders and resources
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->VSSetShader(fullscreenVS.Get(), nullptr, 0);
		context->PSSetShader(radialBlurPS.Get(), nullptr, 0);
		context->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
		context->PSSetShaderResources(0, 1, backBufferCopySRV.GetAddressOf());
		context->PSSetSamplers(0, 1, linearSampler.GetAddressOf());
		
		// Draw fullscreen triangle
		context->Draw(3, 0);
		
		// Restore state
		context->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
		context->RSSetViewports(1, &oldViewport);
		context->VSSetShader(oldVS.Get(), nullptr, 0);
		context->PSSetShader(oldPS.Get(), nullptr, 0);
		
		// Clear shader resources
		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->PSSetShaderResources(0, 1, &nullSRV);
	}

	void InstallHooks()
	{
		// We'll initialize the graphics system when data is loaded
		// The actual Present hook is installed in Initialize() after we have the swap chain
		logger::info("[Graphics] Graphics hooks prepared (initialization deferred until renderer ready)");
	}
}

