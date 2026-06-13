# Nixtaur

Nixtaur is a tool designed to make fetching AUR and Pacman packages usable in Nix-based operating systems.

It bridges the gap between Arch Linux packaging (AUR / pacman) and Nix environments by allowing users to build and install packages in a more familiar Arch-like workflow.

---

## Features

- Install AUR packages in Nix-like systems
- Fetch and handle Pacman/AUR package sources
- Simplifies package building workflows
- Lightweight CLI tool

---

## Why Nixtaur?

As an nix user its very powerful, But when it comes to AUR packages it loses on alot of chances to be better so thats why we made NIXTAUR!

Nixtaur aims to:
- Reduce friction when porting Arch workflows to Nix
- Allow easier access to AUR ecosystem, packages, features.

---

## Requirements

### Core Dependencies
- bash
- curl
- tar
- unzip
- patch
- steam-run *USED TO REPLACE NIX'S FILESYSTEM*

### Optional (for building from source)
- gcc
- make

---

## Installation

OPTION 1 BUILD FROM THE SOURCE

git clone https://github.com/paxxydevv/nixtaur.git
cd NixTaur
make
Run:
./nixtaur --help
Usage

OPTION 2 ALREADY MADE BINARY
Install the latest NixTaur build from the release page.

 Contributing

We welcome contributions from anyone interested in improving Nixtaur.

How to contribute
Fork the repository
Create a new branch:
git checkout -b yourfeatureorsoemthing/yourfeature
Make your changes
Test your changes locally and make sure it has a reason not some random stuff.
Commit your changes:
git commit -m "Add: short description of change"
Push your branch:
git push origin feature/my-feature
Open a Pull Request and wait for the results!

# NOTES
Thank's for checking out our project hope you had a nice exprience :)) for private manners you can DM zapporsumidkk on discord.
Please leave a star if you liked this project.

MIT LICENSE
