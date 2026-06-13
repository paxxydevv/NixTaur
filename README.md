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

NixOS is powerful, but sometimes lacks direct access to AUR-style packages.

Nixtaur aims to:
- Reduce friction when porting Arch workflows to Nix
- Allow easier access to AUR ecosystem
- Provide a familiar pacman-like experience

---

## Requirements

### Core Dependencies
- bash
- curl
- tar
- unzip
- patch
- steam-run *(if used in your workflow)*

### Optional (for building from source)
- gcc
- make

---

## Installation

1. Clone the repository:

git clone https://github.com/<your-username>/nixtaur.git
cd nixtaur
Build (optional):
make
Run:
./nixtaur --help
Usage
./nixtaur --install <package>
./nixtaur --search <package>
./nixtaur --update
🤝 Contributing

We welcome contributions from anyone interested in improving Nixtaur.

How to contribute
Fork the repository
Create a new branch:
git checkout -b feature/my-feature
Make your changes
Test your changes locally
Commit your changes:
git commit -m "Add: short description of change"
Push your branch:
git push origin feature/my-feature
Open a Pull Request
