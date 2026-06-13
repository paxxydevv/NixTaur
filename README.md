# Nixtaur

[![Status](https://img.shields.io/badge/status-working-brightgreen)](https://github.com/paxxydevv/NixTaur)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C++](https://img.shields.io/badge/Language-C%2B%2B-blue.svg)](https://cplusplus.com/)

Nixtaur is a tool designed to make fetching AUR and Pacman packages usable in Nix-based operating systems.

It bridges the gap between Arch Linux packaging (AUR / pacman) and Nix environments by allowing users to build and install packages in a more familiar Arch-like workflow.

## Features

- Install AUR packages in Nix-like systems
- Fetch and handle Pacman/AUR package sources
- Simplifies package building workflows
- Lightweight CLI tool

## Why Nixtaur?

As a Nix user it's very powerful, but when it comes to AUR packages it loses a lot of opportunities to be better, so that's why we made NIXTAUR!

Nixtaur aims to:

- Reduce friction when porting Arch workflows to Nix
- Allow easier access to the AUR ecosystem and packages/features

## Requirements

### Core Dependencies

- bash
- curl
- tar
- unzip
- patch
- steam-run USED TO REPLACE NIX'S FILESYSTEM

### Optional (for building from source)

- gcc
- make

## Installation

### OPTION 1: BUILD FROM SOURCE

```bash
git clone https://github.com/paxxydevv/nixtaur.git
cd NixTaur
make
```

Run:

```bash
./nixtaur --help
```

### OPTION 2: PREBUILT BINARY

Install the latest Nixtaur build from the release page.

## Contributing

We welcome contributions from anyone interested in improving Nixtaur.

### How to contribute

1. Fork the repository
2. Create a new branch:
```bash
git checkout -b yourfeature-or-something/yourfeature
```
3. Make your changes
4. Test your changes locally and make sure it has a reason, not just random stuff
5. Commit your changes:
```bash
git commit -m "Add: short description of change"
```
6. Push your branch:
```bash
git push origin feature/my-feature
```
7. Open a Pull Request and wait for the results

## Notes

Thanks for checking out our project, hope you had a nice experience :))
For private matters you can DM zapporsumidkk on Discord.
Please leave a star if you liked this project.

## License

MIT License
