#include "EnginePCH.h"
#include "JsonArchive.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "TypeSerializer.h"
#include "Asset/AssetManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ObjectSystem/ActorComponentReconstruction.h"

namespace
{
	bool ValidateFloatArray(const json& Value, size_t Count)
	{
		return Value.is_array() && Value.size() == Count &&
			   std::all_of(Value.begin(), Value.end(), [](const json& Element) { return Element.is_number(); });
	}

	bool ValidateProperties(const json& Properties, UClass* Class)
	{
		if (!Properties.is_null() && !Properties.is_object())
			return false;
		try
		{
			for (UClass* Current = Class; Current; Current = Current->Super)
			{
				for (const FProperty& Property : Current->GetProperties())
				{
					if (!Properties.contains(Property.Name))
						continue;
					const json& Value = Properties[Property.Name];
					switch (Property.Type)
					{
					case EPropertyType::Float:
						Value.get<float>();
						break;
					case EPropertyType::Int:
						Value.get<int32>();
						break;
					case EPropertyType::Bool:
						Value.get<bool>();
						break;
					case EPropertyType::String:
						Value.get<FString>();
						break;
					case EPropertyType::Vector:
					case EPropertyType::Rotator:
						if (!ValidateFloatArray(Value, 3))
							return false;
						break;
					case EPropertyType::Vector4:
					case EPropertyType::Color:
						if (!ValidateFloatArray(Value, 4))
							return false;
						break;
					case EPropertyType::Transform:
						if (!Value.is_object() || !ValidateFloatArray(Value.at("Location"), 3) ||
							!ValidateFloatArray(Value.at("Rotation"), 3) || !ValidateFloatArray(Value.at("Scale"), 3))
							return false;
						break;
					case EPropertyType::Object:
						if (!Value.is_null() && !Value.is_string())
							return false;
						break;
					default:
						break;
					}
				}
			}
			if (Properties.contains("OverrideMaterials"))
			{
				if (!Properties["OverrideMaterials"].is_array())
					return false;
				for (const json& Material : Properties["OverrideMaterials"])
				{
					if (Material.is_null())
						continue;
					if (!Material.is_object())
						return false;
					for (const char* Key : {"Asset", "Base", "SamplerState", "BlendState"})
						if (Material.contains(Key))
							Material[Key].get<FString>();
					if (Material.contains("BaseColor") && !ValidateFloatArray(Material["BaseColor"], 4))
						return false;
					if (Material.contains("UVScrollSpeed"))
					{
						if (!ValidateFloatArray(Material["UVScrollSpeed"], 2))
							return false;
					}
					if (Material.contains("Textures"))
					{
						if (!Material["Textures"].is_array())
							return false;
						for (const json& Texture : Material["Textures"])
							if (!Texture.is_null() && !Texture.is_string())
								return false;
					}
				}
			}
		}
		catch (const json::exception&)
		{
			return false;
		}
		return true;
	}

	// "FOV": [60.0] 처럼 배열 하나로 저장된 값과 숫자 하나 모두 받는다.
	float ReadScalar(const json& Value, float Default)
	{
		if (Value.is_number())
			return Value.get<float>();
		if (Value.is_array() && !Value.empty() && Value[0].is_number())
			return Value[0].get<float>();
		return Default;
	}

	// 기본 씬 형식의 "PerspectiveCamera"를 메인 카메라에 적용한다.
	// Rotation은 [Roll, Pitch, Yaw] 라디안이고, 엔진 FRotator는 도 단위다 (Pitch 양수 = 아래를 봄, 씬과 같은 방향).
	void LoadPerspectiveCamera(UWorld* World, const json& CameraJson)
	{
		ACameraActor* CameraActor = World->GetMainCamera();
		UCameraComponent* Camera = CameraActor ? CameraActor->GetCameraComponent() : nullptr;
		if (!Camera || !CameraJson.is_object())
			return;

		if (CameraJson.contains("Location"))
			Camera->SetRelativeLocation(CameraJson["Location"].get<FVector>());

		if (CameraJson.contains("Rotation") && CameraJson["Rotation"].is_array() && CameraJson["Rotation"].size() >= 3)
		{
			const json& R = CameraJson["Rotation"];
			Camera->SetRelativeRotation(FRotator(
				FMath::RadiansToDegrees(R[1].get<float>()),    // Pitch
				FMath::RadiansToDegrees(R[2].get<float>()),    // Yaw
				FMath::RadiansToDegrees(R[0].get<float>())));  // Roll
		}

		if (CameraJson.contains("FOV"))
			Camera->SetFieldOfView(ReadScalar(CameraJson["FOV"], Camera->GetFieldOfView()));
		if (CameraJson.contains("NearClip"))
			Camera->SetNearZ(ReadScalar(CameraJson["NearClip"], Camera->GetNearZ()));
		if (CameraJson.contains("FarClip"))
			Camera->SetFarZ(ReadScalar(CameraJson["FarClip"], Camera->GetFarZ()));
	}
} // namespace

bool FJsonArchive::SaveWorld(UWorld* World, const FString& Path)
{
	if (!World)
		return false;

	ULevel* Level = World->GetPersistentLevel();

	if (!Level)
		return false;

	json Json;

	Json["Version"] = 3;
	Json["Actors"] = json::array();

	for (AActor* Actor : Level->GetActors())
	{
		if (!Actor || Actor->HasAnyFlags(EObjectFlags::RF_Transient))
			continue;

		json ActorJson;
		ActorJson["Class"] = Actor->GetClass()->Name;
		ActorJson["Name"] = Actor->GetName();
		ActorJson["RootComponent"] =
			Actor->GetRootComponent() ? json(Actor->GetRootComponent()->GetName()) : json(nullptr);
		ActorJson["Components"] = json::array();
		Actor->Serialize(ActorJson["Properties"], false);

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component || Component->HasAnyFlags(EObjectFlags::RF_Transient)) continue;
			json ComponentJson;
			ComponentJson["Name"] = Component->GetName();
			ComponentJson["Class"] = Component->GetClass()->Name;
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				USceneComponent* Parent = SceneComponent->GetAttachParent();
				ComponentJson["AttachParent"] = Parent ? json(Parent->GetName()) : json(nullptr);
			}
			Component->Serialize(ComponentJson["Properties"], false);

			ActorJson["Components"].push_back(ComponentJson);
		}

		Json["Actors"].push_back(ActorJson);
	}

	std::ofstream File(Path);

	if (!File.is_open())
		return false;

	File << Json.dump(4);

	File.close();

	return !File.fail();
}

bool FJsonArchive::LoadWorld(UWorld* World, const FString& Path)
{
	if (!World)
		return false;

	if (!std::filesystem::exists(Path))
	{
		HTR_LOG(Warning, "{} is Not Exist!", Path);
		return false;
	}

	std::ifstream File(Path);

	if (!File.is_open())
		return false;

	json Json;

	try
	{
		File >> Json;
	}
	catch (const json::parse_error&)
	{
		return false;
	}

	// 임시 DefaultScene.Scene 로딩용
	if (Json.contains("Primitives"))
	{
		World->ClearWorld();
		for (const auto& [KeyString, PrimJson] : Json["Primitives"].items())
		{
			FTransform Transform;
			Transform.Location = PrimJson["Location"].get<FVector>();
			Transform.Rotation = PrimJson["Rotation"].get<FRotator>();
			Transform.Scale = PrimJson["Scale"].get<FVector>();

			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(NAME_None, &Transform);
			UStaticMesh* Mesh = UAssetManager::GetAssetByPath<UStaticMesh>(PrimJson["ObjStaticMeshAsset"].get<FString>());
			Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		}
		World->GetScene().BuildBVH();

		if (Json.contains("PerspectiveCamera"))
			LoadPerspectiveCamera(World, Json["PerspectiveCamera"]);

		return true;
	}

	if (!Json.contains("Version") || (Json["Version"] != 2 && Json["Version"] != 3))
		return false;
	if (!Json.contains("Actors") || !Json["Actors"].is_array())
		return false;

	// 클래스, 이름, 부모 참조를 검사한 뒤에만 현재 장면을 비운다.
	for (const json& ActorJson : Json["Actors"])
	{
		if (!ActorJson.is_object() || !ActorJson.contains("Class") ||
			!ActorJson["Class"].is_string())
			return false;

		UClass* Class =
			FindClass(ActorJson["Class"].get<FString>());

		if (!Class || !Class->Constructor || !Class->IsChildOf(AActor::StaticClass()))
			return false;
		if (ActorJson.contains("Properties") && !ValidateProperties(ActorJson["Properties"], Class))
			return false;
	if (!ActorJson.contains("Components") || !ActorJson["Components"].is_array())
		return false;
		std::unordered_map<FString, UClass*> ComponentClasses;
		std::unordered_map<FString, FString> Parents;
		for (const json& ComponentJson : ActorJson["Components"])
		{
			if (!ComponentJson.is_object() || !ComponentJson.contains("Name") || !ComponentJson["Name"].is_string() ||
				!ComponentJson.contains("Class") || !ComponentJson["Class"].is_string())
				return false;
			UClass* ComponentClass = FindClass(ComponentJson["Class"].get<FString>());
			if (!ComponentClass || !ComponentClass->Constructor ||
				!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
				return false;
			if (ComponentJson.contains("Properties") &&
				!ValidateProperties(ComponentJson["Properties"], ComponentClass))
				return false;
			if (!ComponentClasses.emplace(ComponentJson["Name"].get<FString>(), ComponentClass).second)
				return false;
			if (ComponentJson.contains("AttachParent") && !ComponentJson["AttachParent"].is_null())
			{
				if (!ComponentClass->IsChildOf(USceneComponent::StaticClass()) ||
					!ComponentJson["AttachParent"].is_string())
					return false;
				Parents.emplace(ComponentJson["Name"].get<FString>(), ComponentJson["AttachParent"].get<FString>());
			}
		}
		for (const auto& [Name, Parent] : Parents)
		{
			auto Found = ComponentClasses.find(Parent);
			if (Found == ComponentClasses.end() || !Found->second->IsChildOf(USceneComponent::StaticClass()))
				return false;
			FString Current = Name;
			for (size_t Depth = 0; Parents.contains(Current); ++Depth)
			{
				if (Depth >= ComponentClasses.size())
					return false;
				Current = Parents.at(Current);
			}
		}
		if (ActorJson.contains("RootComponent") && !ActorJson["RootComponent"].is_null())
		{
			if (!ActorJson["RootComponent"].is_string())
				return false;
			const FString Root = ActorJson["RootComponent"].get<FString>();
			auto Found = ComponentClasses.find(Root);
			if (Found == ComponentClasses.end() || !Found->second->IsChildOf(USceneComponent::StaticClass()) ||
				Parents.contains(Root))
				return false;
		}
	}

	World->ClearWorld();
	for (json& ActorJson : Json["Actors"])
	{
		UClass* Class = FindClass(ActorJson["Class"].get<FString>());
		AActor* Actor = World->SpawnActor(Class);
		if (!Actor)
			return false;
		if (ActorJson.contains("Name") && ActorJson["Name"].is_string())
			Actor->SetName(FName(ActorJson["Name"].get<FString>()));
		Actor->Serialize(ActorJson["Properties"], true);
		TArray<FActorComponentDescriptor> Descriptors;
		for (const json& ComponentJson : ActorJson["Components"])
			Descriptors.Add(
				{FName(ComponentJson["Name"].get<FString>()), FindClass(ComponentJson["Class"].get<FString>())});
		const TArray<UActorComponent*> Components = ReconstructActorComponents(Actor, Descriptors);
		TMap<FString, UActorComponent*> ComponentMap;
		for (int32 Index = 0; Index < Components.Num(); ++Index)
		{
			if (!Components[Index])
				return false;
			ComponentMap.Add(Descriptors[Index].Name.ToString(), Components[Index]);
			Components[Index]->Serialize(ActorJson["Components"][Index]["Properties"], true);
		}

		if (ActorJson.contains("RootComponent"))
		{
			if (!ActorJson["RootComponent"].is_null())
				Actor->SetRootComponent(
					Cast<USceneComponent>(*ComponentMap.Find(ActorJson["RootComponent"].get<FString>())));
		}
		else
		{
			// Version 2에는 계층 정보가 없다. 첫 SceneComponent를 루트로 복원한다.
			for (UActorComponent* Component : Components)
			{
				if (USceneComponent* Scene = Cast<USceneComponent>(Component))
				{
					Actor->SetRootComponent(Scene);
					break;
				}
			}
		}
		for (int32 Index = 0; Index < Components.Num(); ++Index)
		{
			USceneComponent* Scene = Cast<USceneComponent>(Components[Index]);
			if (!Scene)
				continue;
			const json& ComponentJson = ActorJson["Components"][Index];
			if (ComponentJson.contains("AttachParent"))
			{
				if (!ComponentJson["AttachParent"].is_null())
					Scene->SetupAttachment(
						Cast<USceneComponent>(*ComponentMap.Find(ComponentJson["AttachParent"].get<FString>())));
			}
			else if (Scene != Actor->GetRootComponent())
				Scene->SetupAttachment(Actor->GetRootComponent());
		}
		RegisterRestoredActorComponents(Actor);
	}
	World->GetScene().UpdateAllTransforms();

	World->GetScene().BuildBVH();

	return true;
}
