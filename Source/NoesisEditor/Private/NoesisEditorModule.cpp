////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "NoesisEditorModule.h"

// AssetRegistry includes
#include "AssetRegistry/AssetRegistryModule.h"

// Core includes
#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/EngineVersionComparison.h"

// CoreUObject includes
#include "UObject/UObjectGlobals.h"

// Engine includes
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "TextureCompiler.h"

// UnrealEd includes
#include "Editor.h"
#include "EditorReimportHandler.h"

// NoesisRuntime includes
#include "NoesisRuntimeModule.h"
#include "NoesisSettings.h"
#include "NoesisSupport.h"
#include "NoesisTypeClass.h"
#include "NoesisXaml.h"
#include "NoesisRive.h"
#include "NoesisBlueprint.h"
#include "NoesisTextureAssetUserData.h"

// NoesisEditor includes
#include "NoesisBlueprintAssetTypeActions.h"
#include "NoesisXamlAssetTypeActions.h"
#include "NoesisRiveAssetTypeActions.h"
#include "NoesisBlueprintCompiler.h"
#include "NoesisBlueprintCompilerContext.h"
#include "NoesisXamlThumbnailRenderer.h"
#include "NoesisRiveThumbnailRenderer.h"
#include "NoesisStyle.h"
#include "NoesisLangServerResourceProvider.h"

// KismetCompiler includes
#include "KismetCompiler.h"

// LevelEditor includes
#include "LevelEditor.h"

// Settings includes
#include "ISettingsModule.h"

// Slate includes
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

// LangServer includes
#include "NsApp/LangServer.h"

// ContentBrowser includes
#include "ContentBrowserMenuContexts.h"

// ToolMenus includes
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "NoesisEditorModule"

static void PremultiplyAlpha(UTexture2D* Texture)
{
	FTextureSource& TextureSource = Texture->Source;
	const ETextureSourceFormat SourceFormat = TextureSource.GetFormat();
	int32 TextureWidth = TextureSource.GetSizeX();
	int32 TextureHeight = TextureSource.GetSizeY();

	switch (SourceFormat)
	{
	case TSF_BGRA8:
	{
		int32 MipSizeX = TextureWidth;
		int32 MipSizeY = TextureHeight;

		for (int32 MipLevel = 0; MipLevel < TextureSource.GetNumMips(); ++MipLevel)
		{
			uint8* SourceData = TextureSource.LockMip(MipLevel);
			for (int32 Y = 0; Y < MipSizeY; ++Y)
			{
				for (int32 X = 0; X < MipSizeX; ++X)
				{
					uint8* PixelData = SourceData + (Y * MipSizeX + X) * 4;
					uint32 Alpha = (uint32)PixelData[3];
					PixelData[0] = (uint8)(((uint32)PixelData[0] * Alpha) / 255);
					PixelData[1] = (uint8)(((uint32)PixelData[1] * Alpha) / 255);
					PixelData[2] = (uint8)(((uint32)PixelData[2] * Alpha) / 255);
				}
			}
			TextureSource.UnlockMip(MipLevel);

			MipSizeX <<= 2;
			MipSizeY <<= 2;
		}
		break;
	}

	case TSF_RGBA16:
	{
		int32 MipSizeX = TextureWidth;
		int32 MipSizeY = TextureHeight;

		for (int32 MipLevel = 0; MipLevel < TextureSource.GetNumMips(); ++MipLevel)
		{
			uint16* SourceData = (uint16*)TextureSource.LockMip(MipLevel);
			for (int32 Y = 0; Y < MipSizeY; ++Y)
			{
				for (int32 X = 0; X < MipSizeX; ++X)
				{
					uint16* PixelData = SourceData + (Y * MipSizeX + X) * 4;
					uint32 Alpha = (uint32)PixelData[3];
					PixelData[0] = (uint8)(((uint32)PixelData[0] * Alpha) / 255);
					PixelData[1] = (uint8)(((uint32)PixelData[1] * Alpha) / 255);
					PixelData[2] = (uint8)(((uint32)PixelData[2] * Alpha) / 255);
				}
			}
			TextureSource.UnlockMip(MipLevel);

			MipSizeX <<= 2;
			MipSizeY <<= 2;
		}
		break;
	}

	default:
		UE_LOG(LogNoesisEditor, Warning, TEXT("Texture %s format invalid"), *Texture->GetPathName());
		break;
	}
}

static void EnsurePremultiplyAlpha(class UTexture2D* Texture)
{
	auto NoesisTextureData = (UNoesisTextureAssetUserData*)Texture->GetAssetUserDataOfClass(UNoesisTextureAssetUserData::StaticClass());
	if (NoesisTextureData == nullptr)
	{
		NoesisTextureData = NewObject<UNoesisTextureAssetUserData>(Texture, NAME_None, RF_Public | RF_Transactional);

		Texture->PreEditChange(nullptr);
		Texture->AddAssetUserData(NoesisTextureData);
		PremultiplyAlpha(Texture);
		Texture->PostEditChange();
	}
}

static void SetLangServerCallbacks(FNoesisLangServerTextureProvider* textureProvider)
{
	NoesisApp::LangServer::SetDocumentClosedCallback(textureProvider, [](void* user, const Noesis::Uri& uri)
		{
			FNoesisLangServerTextureProvider* provider = (FNoesisLangServerTextureProvider*)user;
			provider->ClearShrinkTextures();
		});
}

static void DestroyThumbnails()
{
	for (TObjectIterator<UNoesisXaml> It; It; ++It)
	{
		UNoesisXaml* Xaml = *It;
		Xaml->DestroyThumbnailRenderData();
	}

	for (TObjectIterator<UNoesisRive> It; It; ++It)
	{
		UNoesisRive* Rive = *It;
		Rive->DestroyThumbnailRenderData();
	}
}

static void OnAssetPostImport(UFactory* ImportFactory, UObject* InObject)
{
	if (!IsValid(InObject))
		return;

	DestroyThumbnails();

	if (InObject->IsA<UNoesisXaml>())
	{
		if (GetDefault<UNoesisSettings>()->ReloadEnabled)
		{
			UNoesisXaml* Xaml = (UNoesisXaml*)InObject;
			INoesisRuntimeModuleInterface::Get().OnXamlChanged(Xaml);
		}
	}

	if (auto Texture = Cast<UTexture2D>(InObject); Texture != nullptr)
	{
		auto NoesisTextureData = (UNoesisTextureAssetUserData*)Texture->GetAssetUserDataOfClass(UNoesisTextureAssetUserData::StaticClass());
		if (NoesisTextureData != nullptr)
		{
			PremultiplyAlpha(Texture);
		}
	}
}

static void OnObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event)
{
	if (!IsValid(Object)) // Don't think this is possible, but better safe than sorry
		return;

	static uint32 ReentryGuard = 0;
	if (!ReentryGuard)
	{
		ReentryGuard = 1;

		DestroyThumbnails();

		if (Object->IsA<UTexture2D>())
		{
			UTexture2D* Texture = (UTexture2D*)Object;
			INoesisRuntimeModuleInterface::Get().OnTextureChanged(Texture);
		}

		if (Object->IsA<UMaterialInterface>())
		{
			NoesisDestroyTypeClassForMaterial((UMaterialInterface*)Object);
		}

		ReentryGuard = 0;
	}
}

static bool CanSetPremultiplyAlpha(const FToolMenuContext& InContext)
{
	if (GetDefault<UNoesisSettings>()->PremultiplyAlpha)
	{
		const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		for (UTexture2D* Texture : CBContext->LoadSelectedObjects<UTexture2D>())
		{
			if (IsValid(Texture))
			{
				auto NoesisTextureData = (UNoesisTextureAssetUserData*)Texture->GetAssetUserDataOfClass(UNoesisTextureAssetUserData::StaticClass());
				if (NoesisTextureData == nullptr)
				{
					return true;
				}
			}
		}
	}
	return false;
}

static void ExecuteSetPremultiplyAlpha(const FToolMenuContext& InContext)
{
	const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
	for (UTexture2D* Texture : CBContext->LoadSelectedObjects<UTexture2D>())
	{
		if (IsValid(Texture))
		{
			EnsurePremultiplyAlpha(Texture);
		}
	}
}

static bool CanResetPremultiplyAlpha(const FToolMenuContext& InContext)
{
	if (GetDefault<UNoesisSettings>()->PremultiplyAlpha)
	{
		const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		for (UTexture2D* Texture : CBContext->LoadSelectedObjects<UTexture2D>())
		{
			if (IsValid(Texture))
			{
				auto NoesisTextureData = (UNoesisTextureAssetUserData*)Texture->GetAssetUserDataOfClass(UNoesisTextureAssetUserData::StaticClass());
				if (NoesisTextureData != nullptr)
				{
					return true;
				}
			}
		}
	}
	return false;
}

static void ExecuteResetPremultiplyAlpha(const FToolMenuContext& InContext)
{
	TArray<UObject*> SelectedObjects;
	const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
	for (UTexture2D* Texture : CBContext->LoadSelectedObjects<UTexture2D>())
	{
		if (IsValid(Texture))
		{
			auto NoesisTextureData = (UNoesisTextureAssetUserData*)Texture->GetAssetUserDataOfClass(UNoesisTextureAssetUserData::StaticClass());
			if (NoesisTextureData != nullptr)
			{
				Texture->RemoveUserDataOfClass(UNoesisTextureAssetUserData::StaticClass());

				SelectedObjects.Add(Texture);
			}
		}
	}

	if (SelectedObjects.Num() > 0)
	{
		FReimportManager::Instance()->ValidateAllSourceFileAndReimport(SelectedObjects, true, INDEX_NONE, false);
	}
}

TSharedPtr<FKismetCompilerContext> GetCompilerForNoesisBlueprint(UBlueprint* Blueprint, FCompilerResultsLog& Results, const FKismetCompilerOptions& CompilerOptions)
{
	UNoesisBlueprint* NoesisBlueprint = CastChecked<UNoesisBlueprint>(Blueprint);
	return TSharedPtr<FKismetCompilerContext>(new FNoesisBlueprintCompilerContext(NoesisBlueprint, Results, CompilerOptions));
}

#if UE_VERSION_OLDER_THAN(5, 0, 0)
typedef FTicker FTSTicker;
#endif

// Forward declare functions used to fix NoesisXaml dependencies
FString GetDependencyPath(const Noesis::Uri& Uri);
void ConfigureFilter(FARFilter& Filter, UClass* Class, bool RecursiveClasses);

class FNoesisEditorModule : public INoesisEditorModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override
	{
		NoesisEditorModuleInterface = this;

		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FNoesisEditorModule::OnPostEngineInit);
	}

#if UE_VERSION_OLDER_THAN(5, 0, 0)
	static bool IsRunningCookCommandlet()
	{
		FString Commandline = FCommandLine::Get();
		const bool bIsCookCommandlet = IsRunningCommandlet() && Commandline.Contains(TEXT("run=cook"));
		return bIsCookCommandlet;
	}
#endif

	void OnPostEngineInit()
	{
		// Register slate style overrides
		FNoesisStyle::Initialize();

		Noesis::Ptr<FNoesisLangServerTextureProvider> LangTextureProvider = Noesis::MakePtr<FNoesisLangServerTextureProvider>();

		// Set LangServer Scheme Providers
		NoesisApp::LangServer::SetXamlProvider(Noesis::MakePtr<FNoesisLangServerXamlProvider>());
		NoesisApp::LangServer::SetTextureProvider(LangTextureProvider);
		NoesisApp::LangServer::SetFontProvider(Noesis::MakePtr<FNoesisLangServerFontProvider>());

		SetLangServerCallbacks(LangTextureProvider);

		// Initialize LangServer
		NoesisApp::LangServer::SetName("Unreal");
		NoesisApp::LangServer::Init();

		// Register asset type actions
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		EAssetTypeCategories::Type Category = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("NoesisGUI")), LOCTEXT("NoesisGUIAssetCategory", "NoesisGUI"));

		NoesisBlueprintAssetTypeActions = MakeShareable(new FNoesisBlueprintAssetTypeActions(Category));
		AssetTools.RegisterAssetTypeActions(NoesisBlueprintAssetTypeActions.ToSharedRef());
		NoesisXamlAssetTypeActions = MakeShareable(new FNoesisXamlAssetTypeActions(Category));
		AssetTools.RegisterAssetTypeActions(NoesisXamlAssetTypeActions.ToSharedRef());
		NoesisRiveAssetTypeActions = MakeShareable(new FNoesisRiveAssetTypeActions(Category));
		AssetTools.RegisterAssetTypeActions(NoesisRiveAssetTypeActions.ToSharedRef());

		// Register blueprint compiler
		NoesisBlueprintCompiler = MakeShareable(new FNoesisBlueprintCompiler());
		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetCompilers().Insert(NoesisBlueprintCompiler.Get(), 0); // Make sure our compiler goes before the WidgetBlueprint compiler
		FKismetCompilerContext::RegisterCompilerForBP(UNoesisBlueprint::StaticClass(), &GetCompilerForNoesisBlueprint);

		// Register settings
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "NoesisGUI",
				LOCTEXT("NoesisSettingsName", "NoesisGUI"),
				LOCTEXT("NoesisSettingsDescroption", "Configure the NoesisGUI plugin."),
				GetMutableDefault<UNoesisSettings>());
		}

		// Register thumbnail renderers
		UThumbnailManager::Get().RegisterCustomRenderer(UNoesisXaml::StaticClass(), UNoesisXamlThumbnailRenderer::StaticClass());
		UThumbnailManager::Get().RegisterCustomRenderer(UNoesisRive::StaticClass(), UNoesisRiveThumbnailRenderer::StaticClass());

		AssetPostImportHandle = GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddStatic(&OnAssetPostImport);

		ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddStatic(&OnObjectPropertyChanged);

		// Register level editor menu
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuBarExtension(
			"Edit",
			EExtensionHook::After,
			NULL,
			FMenuBarExtensionDelegate::CreateStatic(&FNoesisEditorModule::AddNoesisMenu)
		);
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

		// Register ticker
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("NoesisEditor"), 0.0f, [this](float DeltaTime)
		{
			Noesis::GUI::UpdateInspector();
			NoesisApp::LangServer::RunTick();
			return true;
		});

		if (IsRunningCookCommandlet())
		{
			FixNoesisXamlDependencies();
			GetDefault<UNoesisSettings>()->LoadWorldUIXaml();
		}

		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UTexture2D::StaticClass());

		FToolMenuSection& Section = Menu->AddSection("NoesisGUI", LOCTEXT("NoesisGUI", "NoesisGUI"), FToolMenuInsert("GetAssetActions", EToolMenuInsertType::After));
		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			{
				const TAttribute<FText> Label = LOCTEXT("SetPremultiplyAlpha", "Set Premultiply Alpha");
				const TAttribute<FText> ToolTip = LOCTEXT("SetPremultiplyAlphaTooltip", "Multiplies the RGB components of each texel by its A component. This will happen every time the texture is reimported.");
				const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions");

				FToolUIAction UIAction;
				UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteSetPremultiplyAlpha);
				UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanSetPremultiplyAlpha);
				InSection.AddMenuEntry("Noesis_Texture2D_SetPremultiplyAlpha", Label, ToolTip, Icon, UIAction);
			}

			{
				const TAttribute<FText> Label = LOCTEXT("ResetPremultiplyAlpha", "Reset Premultiply Alpha");
				const TAttribute<FText> ToolTip = LOCTEXT("ResetPremultiplyAlphaTooltip", "Reimports the texture to restore the original content.");
				const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions");

				FToolUIAction UIAction;
				UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteResetPremultiplyAlpha);
				UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanResetPremultiplyAlpha);
				InSection.AddMenuEntry("Noesis_Texture2D_ResetPremultiplyAlpha", Label, ToolTip, Icon, UIAction);
			}
		}));
	}

	void FixNoesisXamlDependencies()
	{
		INoesisRuntimeModuleInterface& NoesisRuntime = INoesisRuntimeModuleInterface::Get();
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		UAutomatedAssetImportData* AutomatedAssetImportData = NewObject<UAutomatedAssetImportData>();
		AutomatedAssetImportData->bReplaceExisting = true;
		IFileManager& FileManager = IFileManager::Get();

		FARFilter Filter;
		TArray<FAssetData> XamlAssets;
		ConfigureFilter(Filter, UNoesisXaml::StaticClass(), false);
		AssetRegistryModule.Get().GetAssets(Filter, XamlAssets);

		TArray<FAssetData> MaterialAssets;
		ConfigureFilter(Filter, UMaterialInterface::StaticClass(), true);
		AssetRegistryModule.Get().GetAssets(Filter, MaterialAssets);

		for (auto XamlAsset : XamlAssets)
		{
			auto Xaml = Cast<UNoesisXaml>(XamlAsset.GetAsset());
			if (Xaml == nullptr)
				continue;

			Noesis::MemoryStream XamlStream(Xaml->XamlText.GetData(), (uint32)Xaml->XamlText.Num());

			auto DependencyCallback = [&](const Noesis::Uri& Uri, Noesis::XamlDependencyType Type)
			{
				if (Type == Noesis::XamlDependencyType_Root) return;

				// If the URI is invalid, ImportAssetsAutomated will try to reimport the whole directory
				// causing infinite recursion.
				if (!Uri.IsValid()) return;

				FString Dependency = GetDependencyPath(Uri);

				bool IsUserControl = Type == Noesis::XamlDependencyType_UserControl;
				if (IsUserControl)
				{
					FAssetData* Asset = XamlAssets.FindByPredicate([&Dependency](FAssetData& Asset) { return Asset.AssetName == *Dependency; });
					if (Asset)
					{
						Xaml->Xamls.AddUnique(Cast<UNoesisXaml>(Asset->GetAsset()));
					}
					else
					{
						Asset = MaterialAssets.FindByPredicate([&Dependency](FAssetData& Asset) { return Asset.AssetName == *Dependency; });
						if (Asset)
						{
							Xaml->Materials.AddUnique(Cast<UMaterialInterface>(Asset->GetAsset()));
						}
					}
				}
			};
			auto DependencyCallbackAdaptor = [](void* UserData, const Noesis::Uri& Uri, Noesis::XamlDependencyType Type)
			{
				auto Callback = (decltype(DependencyCallback)*)UserData;
				return (*Callback)(Uri, Type);
			};
			FString Uri = Xaml->GetXamlUri();
			Noesis::GUI::GetXamlDependencies(&XamlStream, TCHAR_TO_UTF8(*Uri), &DependencyCallback, DependencyCallbackAdaptor);
		}
	}

	virtual void ShutdownModule() override
	{
		NoesisEditorModuleInterface = nullptr;

		// Unregister slate style overrides
		FNoesisStyle::Shutdown();

		// Disable LangServer
		NoesisApp::LangServer::Shutdown();

		// Unregister asset type actions
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.UnregisterAssetTypeActions(NoesisBlueprintAssetTypeActions.ToSharedRef());
			AssetTools.UnregisterAssetTypeActions(NoesisXamlAssetTypeActions.ToSharedRef());
			AssetTools.UnregisterAssetTypeActions(NoesisRiveAssetTypeActions.ToSharedRef());
		}

		// Unregister blueprint compiler
		if (FModuleManager::Get().IsModuleLoaded("KismetCompiler"))
		{
			IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
			KismetCompilerModule.GetCompilers().Remove(NoesisBlueprintCompiler.Get());
		}

		// Unregister settings
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "NoesisGUI");
		}

		// Unregister thumbnail renderers
		if (UObjectInitialized())
		{
			UThumbnailManager::Get().UnregisterCustomRenderer(UNoesisXaml::StaticClass());

			if (AssetPostImportHandle.IsValid())
			{
				GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.Remove(AssetPostImportHandle);
			}
		}

		if (ObjectPropertyChangedHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectPropertyChangedHandle);
		}

		// Unregister ticker
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	// End of IModuleInterface interface

	void EnsurePremultiplyAlpha(class UTexture2D* Texture) override
	{
		::EnsurePremultiplyAlpha(Texture);
	}

	static void AddNoesisMenu(FMenuBarBuilder& MenuBarBuilder)
	{
		MenuBarBuilder.AddPullDownMenu(
			LOCTEXT("NoesisMenu", "NoesisGUI"),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateStatic(&FNoesisEditorModule::FillNoesisMenu),
			"Noesis");
	}

	static void FillNoesisMenu(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoesisSettings", "Settings"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FNoesisEditorModule::OpenSettingsDialog))
		);
		MenuBuilder.BeginSection("NoesisLinks", LOCTEXT("NoesisLinks", "External links"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("NoesisDocumentation", "Documentation"),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FNoesisEditorModule::OpenDocumentation))
			);
			MenuBuilder.AddMenuEntry(
				LOCTEXT("NoesisForums", "Forums"),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FNoesisEditorModule::OpenForums))
			);
			MenuBuilder.AddMenuEntry(
				LOCTEXT("NoesisChangeLog", "Release Notes"),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FNoesisEditorModule::OpenChangeLog))
			);
			MenuBuilder.AddMenuEntry(
				LOCTEXT("NoesisBugTracker", "Report a bug"),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FNoesisEditorModule::OpenBugTracker))
			);
		}
		MenuBuilder.EndSection();
		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoesisAbout", "About NoesisGUI"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FNoesisEditorModule::OpenAbout))
		);
	}

	static void OpenSettingsDialog()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->ShowViewer("Project", "Plugins", "NoesisGUI");
		}
	}

	static void OpenDocumentation()
	{
		FPlatformProcess::LaunchURL(TEXT("https://noesisengine.com/docs/Gui.Core.Index.html"), nullptr, nullptr);
	}

	static void OpenForums()
	{
		FPlatformProcess::LaunchURL(TEXT("https://noesisengine.com/forums/"), nullptr, nullptr);
	}

	static void OpenChangeLog()
	{
		FPlatformProcess::LaunchURL(TEXT("https://noesisengine.com/docs/Gui.Core.Changelog.html"), nullptr, nullptr);
	}

	static void OpenBugTracker()
	{
		FPlatformProcess::LaunchURL(TEXT("https://noesisengine.com/bugs/my_view_page.php"), nullptr, nullptr);
	}

	class SNoesisAboutScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNoesisAboutScreen)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			FText Version = FText::Format(LOCTEXT("VersionLabel", "Version: {0}"), FText::FromString(Noesis::GetBuildVersion()));

			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.VAlign(VAlign_Top)
				[
					SNew(SImage)
					.Image(FNoesisStyle::Get()->GetBrush("NoesisEditor.About"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 20.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.Justification(ETextJustify::Center)
					.Text(Version)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(50.0f, 20.0f, 50.0f, 0.0f))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("NoesisChangeLog", "Release Notes"))
					.ButtonColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					.OnClicked(this, &SNoesisAboutScreen::OnReleaseNotesClicked)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(50.0f, 5.0f, 50.0f, 0.0f))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString("Noesis Technologies"))
					.ButtonColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					.OnClicked(this, &SNoesisAboutScreen::OnNoesisTechnologiesClicked)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 30.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
					.Justification(ETextJustify::Center)
					.Text(LOCTEXT("NoesisCopyright2", "(c) 2013 Noesis Technologies S.L. All Rights Reserved."))
				]
			];
		}

	private:

		FReply OnReleaseNotesClicked()
		{
			FPlatformProcess::LaunchURL(TEXT("https://noesisengine.com/docs/Gui.Core.Changelog.html"), nullptr, nullptr);
			return FReply::Handled();
		}

		FReply OnNoesisTechnologiesClicked()
		{
			FPlatformProcess::LaunchURL(TEXT("https://noesisengine.com"), nullptr, nullptr);
			return FReply::Handled();
		}
	};

	static void OpenAbout()
	{
		TSharedRef<SWindow> AboutWindow = SNew(SWindow)
			.Title(LOCTEXT("NoesisAbout", "About NoesisGUI"))
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.SupportsMinimize(false);

		TSharedRef<SWidget> Content = SNew(SNoesisAboutScreen);

		AboutWindow->SetContent(Content);
		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		if (RootWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(AboutWindow, RootWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(AboutWindow);
		}
	}

	static INoesisEditorModuleInterface* NoesisEditorModuleInterface;

private:
	TSharedPtr<FNoesisBlueprintAssetTypeActions> NoesisBlueprintAssetTypeActions;
	TSharedPtr<FNoesisXamlAssetTypeActions> NoesisXamlAssetTypeActions;
	TSharedPtr<FNoesisRiveAssetTypeActions> NoesisRiveAssetTypeActions;
	TSharedPtr<FNoesisBlueprintCompiler> NoesisBlueprintCompiler;
	FDelegateHandle AssetPostImportHandle;
	FDelegateHandle ObjectPropertyChangedHandle;
#if UE_VERSION_OLDER_THAN(5, 0, 0)
	FDelegateHandle TickerHandle;
#else
	FTSTicker::FDelegateHandle TickerHandle;
#endif
};

INoesisEditorModuleInterface* FNoesisEditorModule::NoesisEditorModuleInterface = 0;

INoesisEditorModuleInterface& INoesisEditorModuleInterface::Get()
{
	return *FNoesisEditorModule::NoesisEditorModuleInterface;
}

IMPLEMENT_MODULE(FNoesisEditorModule, NoesisEditor);
DEFINE_LOG_CATEGORY(LogNoesisEditor);

#undef LOCTEXT_NAMESPACE
