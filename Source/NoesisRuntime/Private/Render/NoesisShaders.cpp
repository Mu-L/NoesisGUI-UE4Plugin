////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "NoesisShaders.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisVSConstants, "NoesisVSConstants");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisVSConstantsStereo, "NoesisVSConstantsStereo");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisTextureSize, "NoesisTextureSize");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisPSRgbaConstants, "NoesisPSRgbaConstants");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisPSOpacityConstants, "NoesisPSOpacityConstants");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisPSRadialGradConstants, "NoesisPSRadialGradConstants");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisBlurConstants, "NoesisBlurConstants");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNoesisShadowConstants, "NoesisShadowConstants");

TGlobalResource<FNoesisPosVertexDeclaration> GNoesisPosVertexDeclaration;
TGlobalResource<FNoesisPosColorVertexDeclaration> GNoesisPosColorVertexDeclaration;
TGlobalResource<FNoesisPosTex0VertexDeclaration> GNoesisPosTex0VertexDeclaration;
TGlobalResource<FNoesisPosTex0RectVertexDeclaration> GNoesisPosTex0RectVertexDeclaration;
TGlobalResource<FNoesisPosTex0RectTileVertexDeclaration> GNoesisPosTex0RectTileVertexDeclaration;
TGlobalResource<FNoesisPosColorCoverageVertexDeclaration> GNoesisPosColorCoverageVertexDeclaration;
TGlobalResource<FNoesisPosTex0CoverageVertexDeclaration> GNoesisPosTex0CoverageVertexDeclaration;
TGlobalResource<FNoesisPosTex0CoverageRectVertexDeclaration> GNoesisPosTex0CoverageRectVertexDeclaration;
TGlobalResource<FNoesisPosTex0CoverageRectTileVertexDeclaration> GNoesisPosTex0CoverageRectTileVertexDeclaration;
TGlobalResource<FNoesisPosColorTex1VertexDeclaration> GNoesisPosColorTex1VertexDeclaration;
TGlobalResource<FNoesisPosTex0Tex1VertexDeclaration> GNoesisPosTex0Tex1VertexDeclaration;
TGlobalResource<FNoesisPosTex0Tex1RectVertexDeclaration> GNoesisPosTex0Tex1RectVertexDeclaration;
TGlobalResource<FNoesisPosTex0Tex1RectTileVertexDeclaration> GNoesisPosTex0Tex1RectTileVertexDeclaration;
TGlobalResource<FNoesisPosColorTex0Tex1VertexDeclaration> GNoesisPosColorTex0Tex1VertexDeclaration;
TGlobalResource<FNoesisPosColorTex1RectVertexDeclaration> GNoesisPosColorTex1RectVertexDeclaration;
TGlobalResource<FNoesisPosColorTex0RectImagePosVertexDeclaration> GNoesisPosColorTex0RectImagePosVertexDeclaration;

IMPLEMENT_GLOBAL_SHADER(FNoesisVS, "/Plugin/NoesisGUI/Private/NoesisVS.usf", "NoesisVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FNoesisPS, "/Plugin/NoesisGUI/Private/NoesisPS.usf", "NoesisPS", SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FNoesisMaterialPS, TEXT("/Plugin/NoesisGUI/Private/NoesisMaterialPS.usf"), TEXT("NoesisPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FNoesisCustomEffectPS, TEXT("/Plugin/NoesisGUI/Private/NoesisCustomEffectPS.usf"), TEXT("NoesisPS"), SF_Pixel);
