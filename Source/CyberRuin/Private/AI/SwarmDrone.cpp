#include "AI/SwarmDrone.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ASwarmDrone::ASwarmDrone()
{
	PrimaryActorTick.bCanEverTick = false; // 运动全在 Hive 的 Tick 里，个体零开销

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetSphereRadius(25.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 可被打，不推玩家
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // sweep 防穿墙
	SetRootComponent(Sphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
