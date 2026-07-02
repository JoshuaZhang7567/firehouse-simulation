#include "ThermodynamicFireGrid.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"

AThermodynamicFireGrid::AThermodynamicFireGrid() {
  PrimaryActorTick.bCanEverTick = true;

  USceneComponent* DummyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
  RootComponent = DummyRoot;

  GridWidth = 10;
  GridHeight = 10;
  CellSize = 100.0f;
}

void AThermodynamicFireGrid::BeginPlay() {
  Super::BeginPlay();
  InitializeGrid();
}

void AThermodynamicFireGrid::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);
  PropagateHeat(DeltaTime);

  // ---- VISUAL DEBUG & PARTICLE SPAWNING ----
  for (int32 Y = 0; Y < GridHeight; ++Y) {
    for (int32 X = 0; X < GridWidth; ++X) {
      int32 CurrentIndex = GetIndex(X, Y);
      if (CurrentIndex == INDEX_NONE) continue;

      // Notice the '&' - we are using a reference so we update the actual array!
      FFireCell& Cell = GridCells[CurrentIndex];
      FColor BoxColor = FColor::White;
      bool bShouldDraw = false;

      if (Cell.Fuel > 0.0f) {
          BoxColor = Cell.bIsOnFire ? FColor::Red : FColor::Green;
          bShouldDraw = true;
      } else if (Cell.Temperature > 25.0f) {
          BoxColor = FColor::Orange;
          bShouldDraw = true;
      }

      // --- NEW: SPAWN OR DESTROY PARTICLES ---
      if (Cell.bIsOnFire) {
          if (Cell.FireEffectComponent == nullptr && FireParticleSystem != nullptr) {
              Cell.FireEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                  GetWorld(),
                  FireParticleSystem,
                  Cell.Location,
                  FRotator::ZeroRotator
              );
          }

          // --- NEW: SPAWN CHAR DECAL ---
          if (Cell.CharDecalComponent == nullptr && CharDecalMaterial != nullptr) {
            Cell.CharDecalComponent = UGameplayStatics::SpawnDecalAtLocation(
                GetWorld(),
                CharDecalMaterial,
                // X is the projection depth. 500 means it covers 1000 vertical units! Y and Z keep it locked to your 50x50 cell.
                FVector(500.0f, 50.0f, 50.0f),
                // Spawn it 400 units in the air so it casts the texture straight down over the furniture
                Cell.Location + FVector(0.0f, 0.0f, 400.0f),
                FRotator(-90.0f, 0.0f, 0.0f), // Point it straight down at the furniture
                0.0f
            );
          }
      } else {
          // If it is no longer on fire (burned out), but still has particles running, destroy them!
          if (Cell.FireEffectComponent != nullptr) {
              Cell.FireEffectComponent->DestroyComponent();
              Cell.FireEffectComponent = nullptr;
          }
      }

      // --- DRAW DEBUG SHAPES ---
      if (bShouldDraw) {
          DrawDebugBox(GetWorld(), Cell.Location + FVector(0.0f, 0.0f, 5.0f),
                       FVector(CellSize * 0.48f, CellSize * 0.48f, 5.0f),
                       BoxColor, false, -1.0f, 0, 1.5f);

          FString IndexText = FString::FromInt(CurrentIndex);
          FVector TextLocation = Cell.Location + FVector(0.0f, 0.0f, 15.0f);
          
          DrawDebugString(GetWorld(), TextLocation, IndexText, nullptr, BoxColor, 0.0f, true, 1.2f);
      }
    }
  }
}

void AThermodynamicFireGrid::InitializeGrid() {
  GridCells.Empty();
  FVector ActorLocation = GetActorLocation();

  for (int32 Y = 0; Y < GridHeight; ++Y) {
    for (int32 X = 0; X < GridWidth; ++X) {
      FFireCell NewCell;
      NewCell.Location = ActorLocation + FVector(X * CellSize, Y * CellSize, 5.0f);

      NewCell.Fuel = 0.0f;
      NewCell.Temperature = 20.0f;
      NewCell.bIsOnFire = false;

      FHitResult HitResult;
      FVector TraceStart = NewCell.Location + FVector(0.0f, 0.0f, 200.0f);
      FVector TraceEnd = NewCell.Location - FVector(0.0f, 0.0f, 10.0f);
      FCollisionQueryParams QueryParams;
      QueryParams.AddIgnoredActor(this);

      if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams)) {
          AActor *HitActor = HitResult.GetActor();
          if (HitActor) {
            FString ActorName = HitActor->GetName().ToLower();
            if (!ActorName.Contains(TEXT("floor")) && !ActorName.Contains(TEXT("ground"))) {
              
                // 1. FAST BURN: Fabric (Sofa, Rugs) - Lower fuel pool, burns up fast
                if (HitActor->ActorHasTag(TEXT("Fabric"))) {
                    NewCell.Fuel = 150.0f;
                    NewCell.BurnRate = 2.5f;
                }
                // 2. SLOW BURN: Hardwood (Tables, Bookshelves) - Massive fuel pool, burns very slowly
                else if (HitActor->ActorHasTag(TEXT("Wood")) || HitActor->ActorHasTag(TEXT("Hardwood"))) {
                    NewCell.Fuel = 400.0f;
                    NewCell.BurnRate = 0.3f;
                }
                // 3. DEFAULT: Generic Flammable objects
                else if (HitActor->ActorHasTag(TEXT("Flammable"))) {
                    NewCell.Fuel = 200.0f;
                    NewCell.BurnRate = 1.0f;
                }
            }
          }
      }

      GridCells.Add(NewCell);
    }
  }
  // Ignite a cell near the middle (Index 55) so we can watch it spread
  if (GridCells.Num() > 31) {
    GridCells[31].Temperature = 500.0f;
    GridCells[31].bIsOnFire = true;
    GridCells[31].Fuel = 200.0f;
  }
}

void AThermodynamicFireGrid::TriggerIgnitionAtLocation(FVector WorldLocation, float IgnitionTemperature, float ForcedFuel) {
    FVector ActorLocation = GetActorLocation();
    
    int32 X = FMath::FloorToInt((WorldLocation.X - ActorLocation.X) / CellSize);
    int32 Y = FMath::FloorToInt((WorldLocation.Y - ActorLocation.Y) / CellSize);
    
    int32 TargetIndex = GetIndex(X, Y);
    
    if (TargetIndex != INDEX_NONE) {
        GridCells[TargetIndex].Temperature = IgnitionTemperature;
        GridCells[TargetIndex].bIsOnFire = true;
        
        if (GridCells[TargetIndex].Fuel <= 0.0f) {
            GridCells[TargetIndex].Fuel = ForcedFuel;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Manual ignition triggered successfully at Cell Index: %d (X: %d, Y: %d)"), TargetIndex, X, Y);
    }
}

void AThermodynamicFireGrid::PropagateHeat(float DeltaTime) {
  TArray<FFireCell> NextGridCells = GridCells;

  for (int32 Y = 0; Y < GridHeight; ++Y) {
    for (int32 X = 0; X < GridWidth; ++X) {
      int32 CurrentIndex = GetIndex(X, Y);
      if (CurrentIndex == INDEX_NONE) continue;

      if (GridCells[CurrentIndex].Fuel <= 0.0f && GridCells[CurrentIndex].Temperature <= 20.1f)
        continue;

      // --- NEW RADIATIVE HEAT LOGIC ---
      float HeatGained = 0.0f;

      // Check a 5x5 grid around the cell
      for (int32 dy = -2; dy <= 2; ++dy) {
        for (int32 dx = -2; dx <= 2; ++dx) {
          if (dx == 0 && dy == 0) continue; // Skip self

          int32 NeighborIndex = GetIndex(X + dx, Y + dy);
          if (NeighborIndex != INDEX_NONE) {
            
            // Only absorb heat if the neighbor is HOTTER than this cell
            if (GridCells[NeighborIndex].Temperature > GridCells[CurrentIndex].Temperature) {
                float Distance = FMath::Sqrt((float)(dx * dx + dy * dy));
                float Weight = 1.0f / Distance;
                
                // Accumulate heat based on the temperature difference
                HeatGained += (GridCells[NeighborIndex].Temperature - GridCells[CurrentIndex].Temperature) * Weight * 0.01f * DeltaTime;
            }
          }
        }
      }

      // Apply the accumulated heat directly
      NextGridCells[CurrentIndex].Temperature += HeatGained;
      // --- END NEW RADIATIVE HEAT LOGIC ---

      if (GridCells[CurrentIndex].bIsOnFire) {
        NextGridCells[CurrentIndex].Temperature += 250.0f * DeltaTime;
        if (NextGridCells[CurrentIndex].Temperature > 800.0f) {
          NextGridCells[CurrentIndex].Temperature = 800.0f;
        }

          NextGridCells[CurrentIndex].Fuel -= GridCells[CurrentIndex].BurnRate * DeltaTime;
        if (NextGridCells[CurrentIndex].Fuel <= 0.0f) {
          NextGridCells[CurrentIndex].Fuel = 0.0f;
          NextGridCells[CurrentIndex].bIsOnFire = false;
        }
      } else {
        if (NextGridCells[CurrentIndex].Temperature > 20.0f) {
          NextGridCells[CurrentIndex].Temperature -= 2.0f * DeltaTime;
        }
        if (GridCells[CurrentIndex].Temperature > 150.0f && GridCells[CurrentIndex].Fuel > 0.0f) {
          NextGridCells[CurrentIndex].bIsOnFire = true;
        }
      }
    }
  }
  GridCells = NextGridCells;
}

int32 AThermodynamicFireGrid::GetIndex(int32 X, int32 Y) const {
  if (X >= 0 && X < GridWidth && Y >= 0 && Y < GridHeight) {
    return X + (Y * GridWidth);
  }
  return INDEX_NONE;
}
