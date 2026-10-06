#include "EnginePCH.h"
#include "ObjectDuplication.h"
namespace
{
    void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass)
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
                        DstVal = SrcVal;
                    }
                    // TODO: UActorComponent 계열은 ComponentMap을 이용한 remap이 필요하다.
                    //else if (p.Class->IsChildOf(UActorComponent::StaticClass()))

                    else {

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

    // 2. 원본 Component와 복제본 Component의 대응 관계를 만든다.
    // 기본 Subobject는 기존 Component와 매칭하고, 추가 Component는 새로 생성한다.
    TMap<UActorComponent*, UActorComponent*> ComponentMap;

    for (UActorComponent* SourceComp: SourceActor->GetComponents())
    {
        UActorComponent* MatchedDestComp = nullptr;

        // 생성자가 만든 기본 Subobject인지 Name + Class로 확인한다.
        for (UActorComponent* DestComp : DestActor->GetComponents())
        {
            if (SourceComp->GetFName() == DestComp->GetFName() &&
                SourceComp->GetClass() == DestComp->GetClass())
            {
                MatchedDestComp = DestComp;
                break;
            }
        }
        if (MatchedDestComp)
        {
            // 기존 기본 Subobject끼리 Source -> Dest 대응을 기록한다.
            ComponentMap.Add(SourceComp, MatchedDestComp);
        }
        else
        {
            // 대응되는 기본 Subobject가 없으면 복제본 Actor에 새 Component를 생성한다.
            UActorComponent* NewComp = CastChecked<UActorComponent>(FObjectFactory::ConstructObject(SourceComp->GetClass(), DestActor, NAME_None));
            DestActor->AddOwnedComponent(NewComp);
            ComponentMap.Add(SourceComp, NewComp);
        }
    }

    // 3. 원본 RootComponent에 대응되는 복제본 Component를 Root로 다시 연결한다.
    USceneComponent* SourceRoot = SourceActor->GetRootComponent();
    if (SourceRoot)
    {
        UActorComponent** Found = ComponentMap.Find(SourceRoot);
        if (Found)
        {
            USceneComponent* DestComp = Cast<USceneComponent>(*Found);
            if (DestComp)
            {
                DestActor->SetRootComponent(DestComp);
            }
        }
    }

    // 4. 원본 SceneComponent의 Attachment 관계를 복제본끼리 다시 연결한다.
    for (UActorComponent* SourceComp : SourceActor->GetComponents())
    {
        USceneComponent* SourceScene = Cast<USceneComponent>(SourceComp);
        if (!SourceScene) continue;

        USceneComponent* SourceParent = SourceScene->GetAttachParent();
        if (!SourceParent) continue;

        UActorComponent** FoundDestScene = ComponentMap.Find(SourceScene);
        UActorComponent** FoundDestParent = ComponentMap.Find(SourceParent);

        if (FoundDestScene && FoundDestParent)
        {
            USceneComponent* DestScene = Cast<USceneComponent>(*FoundDestScene);
            USceneComponent* DestParent = Cast<USceneComponent>(*FoundDestParent);

            if (DestScene && DestParent)
            {
                DestScene->SetupAttachment(DestParent);
            }
        }
    }
    
    // 5. Component 상태를 복사한다.
    for (UActorComponent* SourceComp : SourceActor->GetComponents())
    {
        if (SourceComp)
        {
            UActorComponent** DestComp = ComponentMap.Find(SourceComp);
            if (DestComp)
            {
                CopyProperties(SourceComp, *DestComp, SourceComp->GetClass());
            }
        }
    }

    return DestActor;
}

UWorld* FObjectDuplicator::DuplicateWorld(const UWorld* SourceWorld)
{
    if (!SourceWorld) return nullptr;

    // 새 World를 초기화한 뒤 SourceWorld의 Level Actor들을 새 World에 각각 복제한다.
    UWorld* DestWorld = FObjectFactory::ConstructObject<UWorld>();
    if (!DestWorld || !DestWorld->Init())
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