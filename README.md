
# usdtweak

usdtweak is a free and open source editor for Pixar's [USD](https://graphics.pixar.com/usd/release/index.html#) format. usdtweak can already be used for small and simple tasks like cleaning assets, creating and editing layers, inspecting and fixing usd stages. It works on windows, macos and linux.

This project is written in C++ and is powered by [ImGUI](https://github.com/ocornut/imgui) for the UI and [GLFW](https://github.com/glfw/glfw) for the windowing system.

## Sneak peek

https://github.com/cpichard/usdtweak/assets/300243/3f34cd6f-de84-428f-9569-a1ac3bd61206

## Status

usdtweak is a side project and the development is slow and unpredictable as I can only work on it a few hours during the week-end. The original idea behind usdtweak was to improve usdview by adding edition capabilities, for artists, technical directors and users who don't know the USD ascii syntax and are not familiar with python. The current goal driving the developments is to provide at least the same functionalities as usdview with the ability to edit stages and layers.

As of today usdtweak allows

- to browse and edit multiple stages and layers at the same time, copying and pasting specs between layers,
- to edit stages properties in an edit target context
- to create and edit variants at the layer level
- to edit layer hierarchy: adding, deleting, reparenting, and renaming specs
- to edit stage layer stack: adding, deleting new sublayers
- to create and delete compositions like references and payloads, inherits, ...
- to change property values in layers and stages
- to add and delete keys on properties
- to assign material on a prim
- a minimal viewport interaction: translating, rotating, scaling objects.
- text editing (for small files)
- and more ...

If you want to try usdtweak without the burden of compiling it, you can download the latest installer here https://github.com/cpichard/usdtweak/releases. Feel free to [reach out](#contact) if you have any issue with it (or success).

## Building

You can also build usdtweak, [see the instructions](doc/Building.md) in the doc subfolder. 

 - [Building requirements](doc/Building.md#requirements)
 - [Building on windows](doc/Building.md#compiling-on-windows)
 - [Building on macos](doc/Building.md#compiling-on-macos)
 - [Building on linux](doc/Building.md#compiling-on-linux)

## Documentation

The documentation now lives in the growing [wiki](https://github.com/cpichard/usdtweak/wiki), it mainly contains informations on how to use usdtweak. 

## Contributing

This project welcomes any contributions: features ideas, bug fixes, documentation, tutorials, etc. If you want to contribute just [reach out by mail](#contact).

For code contribution you can make a pull request from your fork. If you don't know how to fork and create pull requests, this [video](https://www.youtube.com/watch?v=nT8KGYVurIU) explains the process.

## Known issues

- When enabling the scene materials in Storm, the texture don't always load correctly. This can be solved by setting the USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX environment variable to 1.

## Contact

If you want to know more, or have any issues, questions, drop me an email: cpichard.github@gmail.com or fill a [github issue](https://github.com/cpichard/usdtweak/issues/new).

## Thanks

Big thanks all the humans who contributed to this project, making it better months after months. Special thanks to @oumad for the discussions, design ideas, docs, videos, and much more and @ChubbyQuark for the big push on the wiki which has become really helpful.