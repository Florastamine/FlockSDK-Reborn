## Welcome! 

Flock SDK Reborn is an attempt to reboot [Flock SDK](https://github.com/Florastamine/FlockSDK) from scratch, using the [Urho3D 1.7 codebase](https://github.com/urho3d/Urho3D/tree/1.7) as a starting point. Since the original fork was first born, a lot of bug-fixes, changes and features were introduced into the main Urho3D branch, and due to time/human resource constraints, these changes never made their way into Flock SDK, and thus over time the two codebase slowly drifting apart, which made it even harder to merge stuff. Plus a few decisions to remove certain engine parts which are now needed, and it was decided that starting from scratch is always easier than trying to cobble together missing features from both repository. So... yeah, a rework is now in progress, and will combine both the latest Urho3D master branch and [Flock SDK 0.7.1a](https://github.com/Florastamine/FlockSDK/tree/0.7.1a). This is also one of the main changes which will guide me towards the release of Flock SDK 1.0.0.

## (Incomplete) pre-`v1.0.0` to-do list (in no particular order):

* Re-work the build tools, which still wraps around CMake scripts, but also provides an optional GUI shell, allowing the user to generate a custom .bat or .sh script with the configuration specified in the GUI for building the SDK.
* Write examples for fork-specific new features/changes, and also port various examples from the original engine for completeness and to provide a way of studying the engine API.
* Implement basic game script encryption/decryption, with a private key provided by the user during the compilation process.
* Tools which does compiling the release version of the engine/game, packaging assets, wrapping up game scripts, cleaning up, doing post-compilation steps (UPX compression, etc.).
* Add several more functionalities: numerical integration, name generation, noise generation ([FastNoiseSIMD](https://github.com/Auburns/FastNoiseSIMD)),...
* `std`-based containers (`std::string`, `std::vector<>`, to name a few) will have bigger influence, and will slowly replace the SDK's self-made containers.
* Vietnamese translation for the scene editor.
* Most of platform-dependent APIs are also available for Linux.
* CI re-integration (Travis, AppVeyor)
* Add support for `LightDecalSet` & `SingleDecalSet`.
* Platform deployment tools.
* Steamworks SDK integration.
