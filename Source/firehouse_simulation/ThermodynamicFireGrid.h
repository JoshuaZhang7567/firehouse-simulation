#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "ThermodynamicFireGrid.generated.h"

USTRUCT(BlueprintType)
struct FFireCell {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  FVector Location;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  float Temperature;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  float Fuel;

  // NEW: Every cell tracks how fast it should burn
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  float BurnRate;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  bool bIsOnFire;

  UPROPERTY()
  UNiagaraComponent* FireEffectComponent;
    
  UPROPERTY()
  UDecalComponent* CharDecalComponent;

  FFireCell()
      : Location(FVector::ZeroVector), Temperature(20.0f), Fuel(0.0f), BurnRate(0.0f),
        bIsOnFire(false), FireEffectComponent(nullptr) {}
};

UCLASS(Blueprintable, BlueprintType)
class FIREHOUSE_SIMULATION_API AThermodynamicFireGrid : public AActor {
  GENERATED_BODY()

public:
  AThermodynamicFireGrid();

protected:
  virtual void BeginPlay() override;

public:
  virtual void Tick(float DeltaTime) override;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  int32 GridWidth;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  int32 GridHeight;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  float CellSize;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  UNiagaraSystem *FireParticleSystem;
    
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  UMaterialInterface* CharDecalMaterial;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  TArray<FFireCell> GridCells;

  // Manual gameplay/testing trigger to spark cell indices by 3D coordinates
  UFUNCTION(BlueprintCallable, Category = "Fire Grid Interaction")
  void TriggerIgnitionAtLocation(FVector WorldLocation, float IgnitionTemperature = 500.0f, float ForcedFuel = 150.0f);

private:
  void InitializeGrid();
  void PropagateHeat(float DeltaTime);
  int32 GetIndex(int32 X, int32 Y) const;
};
