#include "EnginePCH.h"
#include "ObjectDuplication.h"
#include "Property.h"
#include "Class.h"
#include "ObjectFactory.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "ActorComponentReconstruction.h"
#include "Component/MeshComponent.h"
#include "Render/Material.h"

namespace
{
    // 공유 에셋은 유지하고, PIE에서 편집할 인스턴스만 분리한다.
    // 같은 원본 인스턴스를 여러 슬롯이 사용하면 복제본에서도 동일한 인스턴스를 참조한다.
    UMaterial* CopyMaterial(UMaterial* Source, TMap<UMaterial*, UMaterial*>& MaterialMap)
    {
        if (!Source || !Source->bIsInstance)
            return Source;

        if (UMaterial** Found = MaterialMap.Find(Source))
            return *Found;

        UMaterial* Copy = UMaterial::CreateInstance(Source);
        Copy->Parent = Source->GetBaseAsset();
        MaterialMap.Add(Source, Copy);
        return Copy;
    }

    // 값은 직접 복사한다. Scene 등록과 프록시 갱신은 모든 참조를 복원한 뒤 한 번에 수행한다.
    void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass,
        const TMap<UActorComponent*, UActorComponent*>& ComponentMap, TMap<UMaterial*, UMaterial*>& MaterialMap)
    {
        for (UClass* c = FromClass; c; c = c->Super)
        {
            for (const FProperty& p : c->Properties)
            {
                void* SrcPtr = reinterpret_cast<char*>(Src) + p.Offset;
                void* DstPtr = reinterpret_cast<char*>(Dst) + p.Offset;

                switch (p.Type)
                {
                case EPropertyType::Float:
                {
                    float& SrcVal = *static_cast<float*>(SrcPtr);
                    float& DstVal = *static_cast<float*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Int:
                {
                    int32& SrcVal = *static_cast<int32*>(SrcPtr);
                    int32& DstVal = *static_cast<int32*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Bool:
                {
                    bool& SrcVal = *static_cast<bool*>(SrcPtr);
                    bool& DstVal = *static_cast<bool*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Vector:
                {
                    FVector& SrcVal = *static_cast<FVector*>(SrcPtr);
                    FVector& DstVal = *static_cast<FVector*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Rotator:
                {
                    FRotator& SrcVal = *static_cast<FRotator*>(SrcPtr);
                    FRotator& DstVal = *static_cast<FRotator*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Vector4:
                {
                    FVector4& SrcVal = *static_cast<FVector4*>(SrcPtr);
                    FVector4& DstVal = *static_cast<FVector4*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Color:
                {
                    FVector4& SrcVal = *static_cast<FVector4*>(SrcPtr);
                    FVector4& DstVal = *static_cast<FVector4*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::String:
                {
                    FString& SrcVal = *static_cast<FString*>(SrcPtr);
                    FString& DstVal = *static_cast<FString*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Transform:
                {
                    FTransform& SrcVal = *static_cast<FTransform*>(SrcPtr);
                    FTransform& DstVal = *static_cast<FTransform*>(DstPtr);
                    DstVal = SrcVal;
                    break;
                }

                case EPropertyType::Object:
                {
                    if (!p.Class) break;

                    if (p.Class->IsChildOf(URenderAsset::StaticClass()))
                    {
                        UObject*& SrcVal = *static_cast<UObject**>(SrcPtr);
                        UObject*& DstVal = *static_cast<UObject**>(DstPtr);
                        if (UMaterial* Material = Cast<UMaterial>(SrcVal))
                            DstVal = CopyMaterial(Material, MaterialMap);
                        else
                        DstVal = SrcVal;
                    }
                    else if (p.Class->IsChildOf(UActorComponent::StaticClass()))
                    {
                        UActorComponent* Source = *static_cast<UActorComponent**>(SrcPtr);
                        UActorComponent* const* Found = ComponentMap.Find(Source);
                        *static_cast<UActorComponent**>(DstPtr) = Found ? *Found : nullptr;
                    }
                    break;
                }

                default:
                    break;
                }
            }
        }
    }
}

AActor* FObjectDuplicator::DuplicateActorToWorld(const AActor* SourceActor, UWorld* DestinationWorld)
{
    if (!SourceActor || !DestinationWorld) return nullptr;

    // 1. 원본과 같은 Actor를 대상 World에 생성한다.
    // SpawnActor를 통해 World / Level / 기본 Subobject를 새 Actor 기준으로 구성한다.
    UClass* Class = SourceActor->GetClass();
    FTransform SpawnTransform = SourceActor->GetActorTransform();

    AActor* DestActor = DestinationWorld->SpawnActor(Class, NAME_None, &SpawnTransform);
    if (!DestActor)
        return nullptr;

    DestActor->SetName(SourceActor->GetFName());

    // 생성자의 기본 구성과 편집된 구성을 맞춘 뒤 원본/복제본 대응표를 만든다.
    // UUID 같은 임시 표시 컴포넌트는 복제하지 않는다.
    TArray<UActorComponent*> SourceComponents;
    for (UActorComponent* Component : SourceActor->GetComponents())
        if (!Component->HasAnyFlags(EObjectFlags::RF_Transient))
            SourceComponents.Add(Component);

    TArray<FActorComponentDescriptor> Descriptors;
    for (UActorComponent* SourceComp : SourceComponents)
        Descriptors.Add({SourceComp->GetFName(), SourceComp->GetClass()});

    const TArray<UActorComponent*> DestComponents = ReconstructActorComponents(DestActor, Descriptors);
    TMap<UActorComponent*, UActorComponent*> ComponentMap;

    for (int32 Index = 0; Index < DestComponents.Num(); ++Index)
    {
        if (!DestComponents[Index])
            return nullptr;
        ComponentMap.Add(SourceComponents[Index], DestComponents[Index]);
    }

    if (UActorComponent** Root = ComponentMap.Find(SourceActor->GetRootComponent()))
        DestActor->SetRootComponent(Cast<USceneComponent>(*Root));

    // 상대 Transform은 그대로 복사하고, 부모는 대상 World의 컴포넌트로 연결한다.
    TMap<UMaterial*, UMaterial*> MaterialMap;
    CopyProperties(const_cast<AActor*>(SourceActor), DestActor, Class, ComponentMap, MaterialMap);
    for (UActorComponent* SourceComp: SourceComponents)
    {
        UActorComponent* DestComp = *ComponentMap.Find(SourceComp);
        assert(SourceComp != DestComp);
        CopyProperties(SourceComp, DestComp, SourceComp->GetClass(), ComponentMap, MaterialMap);
    if (USceneComponent* SourceScene = Cast<USceneComponent>(SourceComp))
        {
            UActorComponent** Parent = ComponentMap.Find(SourceScene->GetAttachParent());
            Cast<USceneComponent>(DestComp)->SetupAttachment(Parent ? Cast<USceneComponent>(*Parent) : nullptr);
        }

        // 슬롯별 머티리얼은 리플렉션 밖에 있으므로 별도로 복원한다.
        if (UMeshComponent* SourceMesh = Cast<UMeshComponent>(SourceComp))
        {
            UMeshComponent* DestMesh = Cast<UMeshComponent>(DestComp);
            DestMesh->ClearOverrideMaterials();
            for (int32 Slot = 0; Slot < SourceMesh->GetNumOverrideMaterials(); ++Slot)
                DestMesh->SetMaterial(Slot, CopyMaterial(SourceMesh->GetOverrideMaterial(Slot), MaterialMap));
            }
        else if (UPrimitiveComponent* SourcePrimitive = Cast<UPrimitiveComponent>(SourceComp))
    {
            UPrimitiveComponent* DestPrimitive = Cast<UPrimitiveComponent>(DestComp);
            for (int32 Slot = 0; Slot < SourcePrimitive->GetNumMaterials(); ++Slot)
                DestPrimitive->SetMaterial(Slot, CopyMaterial(SourcePrimitive->GetMaterial(Slot), MaterialMap));
            }
        }
    // 멤버 직접 복사는 setter를 거치지 않으므로 Transform과 렌더 상태도 명시적으로 갱신한다.
    RegisterRestoredActorComponents(DestActor);

    return DestActor;
}

UWorld* FObjectDuplicator::DuplicateWorld(const UWorld* SourceWorld, EWorldType InType)
{
    if (!SourceWorld) return nullptr;


    // 새 World를 초기화한 뒤 SourceWorld의 Level Actor들을 새 World에 각각 복제한다.
    UWorld* DestWorld = FObjectFactory::ConstructObject<UWorld>();
    if (!DestWorld || !DestWorld->Init(InType))
    {
        delete DestWorld;
        return nullptr;
    }

    for (AActor* SourceActor : SourceWorld->GetPersistentLevel()->GetActors())
    {
        if (DuplicateActorToWorld(SourceActor, DestWorld) == nullptr)
        {
            HTR_LOG(Error, "Failed to duplicate Actor while duplicating World");
            delete DestWorld;
            return nullptr;
        }
    }
    return DestWorld;
}
