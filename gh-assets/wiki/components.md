The Engine has a compositional object system. `GameObject`s have "base" APIs (properties, methods, events) that can be extended by giving them "components." These are transparent to scripts, adding a Component immediately makes their properties etc accessible via the Object. All components, as well as the base API, are described below.


## `GameObject`

### Properties:
* `Enabled: boolean`: Whether the Engine updates and recognizes this Object, essentially freezing it if it is disabled
* `Exists: boolean `: Whether or not the Object still exists. Will be false after calling `:Destroy`
* `Name: string`: The name of the Object, usually what was passed into `GameObject.new`
* `ObjectId: number `: The ID of the Object, an integer. Remains the same throughout the session, but not guaranteed between sessions
* `Parent: (GameObject & any)?`: The hierarchal parent of the Object, or `nil` if it does not have one
* `Serializes: boolean`: Whether or not the Object should be saved with the scene if it is serialized with `scene.save`, and whether `:Duplicate` will duplicate it (only applies to descendants)

### Methods:
* `AddComponent(keyof<Creatables>) : ()`: Adds the specified Component to the Object. Will error if it already has it (use `:HasComponent` to check beforehand)
* `Destroy() : ()`: Marks the Object as "pending for destruction". This may or may not remove the Object from memory immediately, depending on whether the Engine is keeping a internal reference to it
* `Duplicate() : ((GameObject & any))`: Creates a duplicate of the Object
* `FindChild(string) : ((GameObject & any)?)`: Returns a child Object with the given name, or `nil` if one doesn't exist
* `FindChildWithComponent(keyof<Creatables>) : (GameObject & any)`: Returns a child Object which has the given component, or `nil` if one doesn't exist
* `ForEachChild((any) -> (any)) : ()`: For all children of the Object, invokes the callback. If the callback explicitly returns `false`, iteration ends early. Callback cannot yield
* `ForEachDescendant((any) -> (any)) : ()`: For all descendants of the Object, invokes the callback. If the callback explicitly returns `false`, iteration ends early. Callback cannot yield
* `GetChildren() : ({ any })`: Returns a list of the Object's direct children
* `GetComponents() : (keyof<Creatables>)`: Returns a list of the names of all Components that the Object currently has
* `GetDescendants() : ({ any })`: Returns all descendants of the Object (i.e. its children, and their children, etc)
* `GetFullName() : (string)`: Returns the full, hierarchal path of the Object
* `HasComponent(keyof<Creatables>) : (boolean)`: Returns whether or not the Object has the given Component
* `MergeWith((GameObject & any)) : ()`: Merges the Objects together, replacing descendants with the same name
* `RemoveComponent(keyof<Creatables>) : ()`: Removes the specified Component from the Object. Will error if does not have it (use `:HasComponent` to check beforehand)

## `Animation`

* Represents an Animation
* Not functional currently

### Properties:
* `Animation: string`: The animation file
* `Looped: boolean`: Whether the Animation should loop when it reaches the end
* `Playing: boolean`: Whether the Animation is playing
* `Ready: boolean `: Whether the Animation can be played
* `Weight: number`: The influence the Animation has over the rig

## `AssetManager`

* Used to manipulate assets within the Engine


### Methods:
* `GetMeshData(string) : ({ [string]: any })`: Returns the provided mesh data associated with the provided path
* `ImportModel(string) : ((GameObject & any))`: Imports the glTF 2.0 model at the provided path and returns it as a `Model` GameObject
* `SaveMesh(string, string) : ()`: Saves the mesh data at the provided path to a file
* `SetMeshData(self, Id: string, MeshData: { Vertices: { { Position: vector, Normal: vector, Paint: { R: number, G: number, B: number, A: number }, UV: { number } } }, Indices: { number } }) : ()`: Associates the provided mesh data with the provided path

## `Bone`

* Represents a Bone of a Mesh
* Created automatically when the asset of a `Mesh` component finishes loading

### Properties:
* `IsActive: boolean `: Whether or not the Bone Object is actively affecting a `Mesh`
* `SkeletalBoneId: number `: The internal ID of the bone, valid range is 0-255
* `TargetMesh: (GameObject & any) `: The `Mesh` component containing Object the Bone is currently targeting. Note that this property is read-only, and will only either be equal to the Mesh that created the Bone, or `nil`
* `Transform: Matrix`: The transformation of the Bone

## `Camera`

* A 3D camera

### Properties:
* `FieldOfView: number`: The field-of-view. By default, `90`
* `UseSimpleController: boolean`: Whether it is affected by the built-in controller (`W`/`A`/`S`/`D` horizontal movement, `Q`/`E` vertical, Mouse to look around)

## `DataModel`

* The root of the scene

### Properties:
* `IsModuleBound: boolean `: If this is `true`, attempting to change `.Module` will throw an error
* `Module: string`: The Module associated with the DataModel that will be run upon the DataModel being bound
* `Time: number `: Number of seconds since the Engine started running

### Methods:
* `GetService(string) : ((GameObject & any))`: Gets the Object representing the specified Service. If it does not exist yet in the Data Model, it is created

### Events:
* `OnFrameBegin(number)`: Fires at the beginning of a frame, passing in the time elapsed since the last frame (delta time)

## `DirectionalLight`

* Intended to model a large light source infinitely far away, like the Sun

### Properties:
* `Brightness: number`: How bright the Light is
* `Direction: vector`: The direction light is emitted from
* `LightColor: Color`: The color of the Light
* `ShadowViewDistance: number`: How far back the shadow view is offset from the focus
* `ShadowViewFarPlane: number`: The Far Z plane of the shadow view. Should be greater than `ShadowViewNearPlane`
* `ShadowViewMoveWithCamera: boolean`: Sets the focus of the shadow view to be the Scene Camera
* `ShadowViewNearPlane: number`: The Near Z plane of the shadow view. Should be less than `ShadowViewFarPlane`. Shouldn't be `0`
* `ShadowViewOffset: vector`: Offset of the shadow view from the focus, after `ShadowViewDistance` is applied
* `ShadowViewSize: number`: Size of the shadow view. Sets `ShadowViewSizeH` and `ShadowViewSizeV` to the assigned value if they are equal. Is `-1` if they are not equal
* `ShadowViewSizeH: number`: Horizontal size of the shadow view
* `ShadowViewSizeV: number`: Vertical size of the shadow view
* `Shadows: boolean`: Whether the Light can render shadows

## `Engine`

* Inspect and manipulate various parts of the Engine

### Properties:
* `BoundDataModel: (GameObject & any) `: The DataModel object that is currently bound
* `CommitHash: string `: The Git commit that was used to produce this Engine build
* `DebugWireframeRendering: boolean`: Whether the Wireframe rendering debug mode is enabled
* `Framerate: number `: The Engine's current framerate
* `FramerateCap: number`: The maximum framerate the Engine will reach before attempting to self-limit
* `IsFullscreen: boolean`: Whether the Window is currently fullscreened
* `IsHeadless: boolean `: Whether the Engine is currently running in headless mode
* `VSync: boolean`: Whether Vertical Synchronization is enabled
* `Version: string `: The version of the Engine build
* `WindowSize: vector`: The size of the window, in pixels

### Methods:
* `BindDataModel( GameObject & DataModel ) : ()`: Switches Data Models to the specified Object
* `CloseVM(string) : ()`: Closes the specified Luau VM, releasing its resources
* `CreateVM(string) : ()`: Creates a new Luau VM with the given name. The `workspace` and `game` globals of the VM are taken from the currently-bound DataModel
* `Exit(number?) : ()`: Shuts down the Engine and exits the process with the specified Exit Code (or 0 by default)
* `GetCliArguments() : ({ any })`: Returns the list of launch arguments given to the Engine. The first item is always a path to the Engine executable
* `GetCurrentVM() : (string)`: Returns the name of the currently-bound VM
* `GetExplorerSelections() : ({ GameObject })`: Returns what is currently selected in the Explorer
* `GetToolNames() : { string }`: Returns a list of valid Engine tools
* `IsToolEnabled(string) : (boolean)`: Returns whether the given tool is enabled
* `OpenTextDocument(string, number?) : ()`: Opens a text file
* `ResetViewport() : ()`: Resets the Engine's understanding of where the viewport is on screen back to the default (occupying the entire window)
* `RunInVM(string, string, string?) : (boolean, string?)`: Runs the given code in the specified VM. Returns `false` and an error message upon failure or if the code does not finish executing (yield or break)
* `SetCurrentVM(string) : ()`: Binds the given VM as the "current" one. This causes all Scripts which load afterward to use the specified VM, it does *not* change the VM of any Scripts loaded beforehand
* `SetExplorerRoot((GameObject & any)) : ()`: Changes the Root of the built-in Explorer's hierarchy view
* `SetExplorerSelections({ GameObject }) : ()`: Changes the built-in Explorer's Object selections to the specified Objects
* `SetToolEnabled(string, boolean) : ()`: Enabled/disables the given tool
* `SetViewport(vector, vector) : ()`: Configures the Engine to act as through the Viewport is at the given position with the given size. Does *not* change where the viewport is actually rendered
* `ShowMessageBox(string, string, string?, string?, number?) : (number)`: Shows a message box to the player with the given specification. The returned number with be the selected option: `0` for Cancel/No, `1` for Ok/Yes, `2` for No in `yesnocancel`. This isa blocking operation and no scripts or other parts of the Engine run until the message is dismissed. Avoid using quotes and backticks in the message
* `UnloadTexture(string) : ()`: Removes the given texture from the Engine's cache, forcing it to be reloaded upon next use

### Events:
* `OnMessageLogged(number, string, string)`: Fires whenever a Log Message is created

## `Example`

* An example to be made fun of and bullied
* The least-popular kid in school no one remembers the name of

### Properties:
* `EvenMoreSecretMessage: string `: A secret message that's so, *so* secret that it cannot be read by Reflection-kun, almost like they're keeping a secret from her...
* `SecretMessage: string `: A secret message that's so important it's read-only
* `SomeInteger: number`: An integer
* `SuperCoolBool: boolean`: A boolean
* `WhereIAm: vector`: A 3-dimensional vector quantity

### Methods:
* `GiveUp() : ()`: Tells Engine-chan to kill herself and throws an exception
* `Greet({ string }, boolean) : string`: Says hello to the given list of names. If the second argument is `true`, does so extra villainously

### Events:
* `OnGreeted(string, string)`: Fires when `:Greet` is called, and sends the response to the callback before the original caller of `:Greet` receives it

## `History`

* Service for implementing Undo/Redo systems

### Properties:
* `ActionHistorySize: number `: The total number of Actions that have finished being recorded
* `CurrentActionName: string? `: The name of the Action currently being recorded, or `nil` if no recording is in progress
* `CurrentWaypoint: number `: The ID of the Action that was last finished
* `IsRecordingAction: boolean `: Whether an Action is currently being recorded by History
* `TargetDataModel: (GameObject & any)`: The DataModel that History will track

### Methods:
* `CanRedo() : (boolean)`: Returns whether `:Redo` will succeed
* `CanUndo() : (boolean)`: Returns whether `:Undo` will succeed
* `ClearHistory() : ()`: Clears all recorded history
* `DiscardAction(number) : ()`: Discards the Action by its ID
* `EnableRecording() : ()`: Enables the main functionality of this service
* `FinishAction(number) : ()`: Finishes the Action by its ID. Only one Action may be active at a time
* `GetActionData(number) : ({ [string]: any })`: Retrives information about a previously-recorded Action by its ID (IDs are in the range 0 <= ID < `ActionHistorySize`)
* `GetCurrentActionData() : ({ [string]: any }?)`: Retrieves information about the current Action being recorded
* `Redo() : ()`: Forwards the state of the scene to how it was before the last Undo
* `TryBeginAction(string) : (number?)`: Attempts to begin an "Action." Returns the ID of the action. May fail and return `nil`. Only one Action may be active at a time
* `Undo() : ()`: Reverses the state of the scene to how it was before the last Action began

## `Mesh`

* A mesh composed of triangles

### Properties:
* `CastsShadows: boolean`: Whether it casts shadows
* `FaceCulling: number`: Which faces of the mesh to cull based on viewing direction. See the `FaceCulling` enum
* `Material: string`: The name of the `.mtl` in the `resources/materials/` directory it should use
* `MeshAsset: string`: The path to the underlying `.hxmesh` file, or a built-in identifier like `!Quad` or `!Cube`
* `MetallnessFactor: number`: Metallness modifier
* `RoughnessFactor: number`: Roughness modifier
* `Tint: Color`: The Color it should be tinted with
* `Transparency: number`: Its transparency/translucency

## `Model`

* A container

No members defined

## `ParticleEmitter`

* An emitter of 2D particles

### Properties:
* `Emitting: boolean`: Whether the emitter should emit new particles
* `Lifetime: vector`: The X and Y components act as a range of how long any particle can last, in seconds
* `ParticlesAreAttached: boolean`: Whether the particles are attached to and move with the emitter
* `Rate: number`: An integer indicating how many particles should be emitted per second (must be above or equal to `0`)

## `Physics`

* Manipulate the Physics simulation of the Engine

### Properties:
* `DebugCollisionAabbs: boolean`: Whether physics collision AABBs are rendered
* `DebugContactPoints: boolean`: Whether Contact Points are rendered
* `DebugSpatialHeat: boolean`: Whether Spatial Heat (spatial hash density) debug rendering is enabled
* `Gravity: vector`: The gravitational field strength
* `Simulating: boolean`: Whether the Physics simulation is running. IMPORTANT: The Editor sets this value to `true` and `false` when starting/stopping playtests
* `Timescale: number`: Slow-down or speed-up the simulation by changing this value from `1`

## `PlayerInput`

* Checking player inputs

### Properties:
* `CursorMode: number`: The current mode/behaviour of the cursor. Refer to the `Enum.CursorMode` enumeration
* `IsKeyboardSunk: boolean `: Whether the Engine is currently using keyboard input (such as for Dear ImGui). Do not process keyboard input if this is `true`
* `IsMouseSunk: boolean `: Whether the Engine is currently using mouse input (such as for Dear ImGui). Do not process mouse input if this is `true`

### Methods:
* `GetCursorPosition() : (vector)`: Returns the current position of the mouse cursor relative to the window as a `vector`
* `IsKeyPressed(number) : (boolean)`: Returns whether the specified key (e.g. `Enum.Key.A`, `Enum.Key.One`) is currently being pressed
* `IsMouseButtonPressed(number) : (boolean)`: Returns whether the specified mouse button (`Enum.MouseButton.Left/Middle/Right`) is currently being pressed

## `PointLight`

* A light source emitting light omnidirectionally from its `Transform`

### Properties:
* `Brightness: number`: How bright the Light is
* `LightColor: Color`: The color of the Light
* `Range: number`: How far light is emitted. If `>= 0`, the Range is used and attenuation is linear, otherwise it uses the formula `F = 1/D^2 * B` to mirror real-world attenuation, where `F` is the final brightness, `D` is the distance of a point from the Light, and `B` is the `Brightness` property's value

## `RigidBody`

* Something rigid that participates in the Physics simulation

### Properties:
* `AngularVelocity: vector`: Its rotational velocity
* `CollisionFidelity: number`: The fidelity at which collisions are detected. See the `CollisionFidelity` enum
* `Density: number`: Its density (`Mass = Density * Size`)
* `Elasticity: number`: Elasticity
* `Friction: number`: Fraction of Velocity it should lose per second while in contact with another object while `PhysicsDynamics` is `true`
* `GravityFactor: number`: Influence of the force of gravity
* `LinearVelocity: vector`: Its velocity
* `PhysicsCollisions: boolean`: Whether other physics objects can collide with it
* `PhysicsDynamics: boolean`: Whether the Physics engine should apply forces to it
* `PhysicsRotations: boolean`: Whether the Physics engine will apply angular impulses to rotate the object on collisions
* `Restitution: number`: Restitution

## `Sound`

* A sound

### Properties:
* `FinishedLoading: boolean `: Whether the Sound has finished loading the `SoundFile`
* `Length: number `: The length of the sound in seconds
* `LoadSucceeded: boolean `: Whether or not the sound loaded successfully
* `Looped: boolean`: Whether playback should loop once it reaches the end
* `Playing: boolean`: Whether the Sound is currently playing (or requested to play, if `.Playing` is set to `true` by a `Script` before it has finished loading)
* `Position: number`: The time-position of playback in number of seconds from the beginning of the file
* `SoundFile: string`: The sound file to be played
* `Speed: number`: The speed at which the sound plays, within the range of 0.01 to 100
* `Volume: number`: The volume at which the sound plays. Must be positive

### Methods:
* `Play() : ()`: Plays the Sound from the beginning

### Events:
* `OnLoaded(boolean)`: Fires when the Sound finishes loading, and whether it loaded successfully or not

## `SpotLight`

* A light which emits in a conical shape

### Properties:
* `Angle: number`: The Field-of-View (in radians) of light emission
* `Brightness: number`: How bright the Light is
* `LightColor: Color`: The color of the Light
* `Range: number`: How far light is emitted

## `Transform`

* Gives a physical position and size to an Object

### Properties:
* `LocalSize: vector `: The Object's scale factor, relative to it's parent Transform ancestor or the world origin
* `LocalTransform: Matrix `: The Object's transformation, relative to it's parent Transform ancestor or the world origin
* `Size: vector`: The Object's world-space scale factor
* `Transform: Matrix`: The Object's world-space transformation

## `TreeLink`

* The Engine, for rendering and physics, will pretend the children of this Object's `Target` are the children of the node
* This is *not* intended to act like a transparent proxy

### Properties:
* `Target: (GameObject & any)?`: The target to link

## `Workspace`

* The container used for storing the main parts of a scene (3D objects)
* If this is `Destroy`'d, the application closes

### Properties:
* `SceneCamera: GameObject & Camera`: The `Camera` the Player sees the world through. If set to `nil`, a *fallback* is created at the origin

### Methods:
* `GetObjectsInAabb(vector, vector, { any }?) : ({ any })`: Get a list of Objects whose bounds are within the AABB
* `Raycast(vector, vector, { any }?, boolean?) : ({ [string]: any }?)`: Cast a ray
* `ScreenPointToVector(vector, number?) : (vector)`: Converts the provided screen coordinates to a world-space direction. `Length` is optional and `1.0` by default

