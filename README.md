# Firehouse Simulation

A thermodynamic fire propagation simulation built with C++ and Substrate in Unreal Engine.

## Prerequisites

Before cloning this repository, you must have the following installed on your machine:

* **Unreal Engine 5** (Ensure you have the matching version used for this project)
* **Git** * **Git LFS (Large File Storage)** - This is required to download the heavy 3D assets, textures, and maps.

## Initial Setup & Cloning

Do not use the standard `git clone` command until Git LFS is active on your machine, or the `Content` folders will fail to download correctly.

**1. Install Git LFS**

* **Mac:** Run `brew install git-lfs` in your terminal.
* **Windows:** Download the installer from [git-lfs.com](https://git-lfs.com/) and run it.
* Once installed, run the following command in your terminal to initialize it:

```bash
git lfs install

```

**2. Clone the Repository**
Download the project to your local machine:

```bash
git clone https://github.com/JoshuaZhang7567/firehouse-simulation.git

```

**3. Rebuild the Local Binaries**
To keep the repository fast and lightweight, temporary build files have been excluded. Your machine needs to generate them before the engine can open.

* Navigate into the cloned `firehouse_simulation` folder.
* **Windows:** Right-click `firehouse_simulation.uproject` and select **Generate Visual Studio project files**.
* **Mac:** Right-click `firehouse_simulation.uproject`, select **Services**, and click **Generate Xcode project**.
* Open the generated IDE project file, compile the C++ code, and then launch the `.uproject` file.

## What to Commit (And What to Ignore)

Unreal Engine generates massive temporary files specific to your local hardware. To ensure you only commit the correct files and ignore the bloat, strictly use the following commands when staging your work:

```bash
git add Content/
git add Source/
git add Config/
git add *.uproject
git add .gitignore .gitattributes
git commit -m "Your commit message here"

```

**NEVER manually add or commit these generated folders (they will automatically rebuild locally):**

* `Binaries/`
* `Intermediate/`
* `DerivedDataCache/`
* `Saved/`
* `.xcworkspace`, `.sln`, or `.idea` files

## Collaboration Rules (Crucial)

Unreal Engine handles different file types in specific ways. To prevent permanent data loss, all contributors must follow this workflow:

### Safe to Merge (C++ Code & Configs)

Files in the `Source/` and `Config/` folders are standard text files. Multiple people can edit these simultaneously, and Git will automatically merge the changes without issue.

### NEVER Merge (Blueprints, Maps, Materials)

Files in the `Content/` folder (like `.uasset` and `.umap` files) are binary files. **Git cannot merge them.** If two people edit the same Blueprint or Material at the same time, one person's work will be permanently overwritten.

* **The "Lock" Rule:** Before opening and modifying any map, material, or blueprint, communicate with the team (e.g., *"I am working on the Fire Decal Material, do not touch it"*).
* Always pull the latest branch (`git pull origin main`) before starting your work session to ensure you have the newest assets.
